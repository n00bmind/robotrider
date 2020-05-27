/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if NON_UNITY_BUILD
#include "math_types.h"
#include "math_sdf.h" 
#include "renderer.h"
#include "meshgen.h"
#include "world.h"
#endif


IsoSurfaceSamplingCache InitSurfaceSamplingCache( MemoryArena* arena, v2u const& cellsPerAxis )
{
    IsoSurfaceSamplingCache result;
    result.cellsPerAxis = cellsPerAxis;

    // NOTE We add an extra row at the ends to account for the edges at the extremes,
    // which simplifies the algorithm by eliminating "edge" cases ¬¬
    v2u stepsPerAxis = cellsPerAxis + V2uOne;
    u32 layerCellCount = stepsPerAxis.x * stepsPerAxis.y;

    result.bottomLayerSamples = PUSH_ARRAY( arena, r32, layerCellCount );
    result.topLayerSamples = PUSH_ARRAY( arena, r32, layerCellCount );
    result.bottomLayerVertexIndices = PUSH_ARRAY( arena, u32, layerCellCount * 2 );
    result.middleLayerVertexIndices = PUSH_ARRAY( arena, u32, layerCellCount );
    result.topLayerVertexIndices = PUSH_ARRAY( arena, u32, layerCellCount * 2 );

    return result;
}

void ClearVertexCaches( IsoSurfaceSamplingCache* samplingCache, bool clearBottomLayer )
{
    v2u stepsPerAxis = samplingCache->cellsPerAxis + V2uOne;
    u32 layerCellCount = stepsPerAxis.x * stepsPerAxis.y;

    if( clearBottomLayer )
    {
        PSET( samplingCache->bottomLayerVertexIndices, U32MAX,
             layerCellCount * 2 * sizeof(u32) );
    }
    PSET( samplingCache->middleLayerVertexIndices, U32MAX,
         layerCellCount * sizeof(u32) );
    PSET( samplingCache->topLayerVertexIndices, U32MAX, 
         layerCellCount * 2 * sizeof(u32) );
}

void SwapTopAndBottomLayers( IsoSurfaceSamplingCache* samplingCache )
{
    IsoSurfaceSamplingCache copy = *samplingCache;
    samplingCache->bottomLayerSamples = copy.topLayerSamples;
    samplingCache->topLayerSamples = copy.bottomLayerSamples;
    samplingCache->bottomLayerVertexIndices = copy.topLayerVertexIndices;
    samplingCache->topLayerVertexIndices = copy.bottomLayerVertexIndices;
}


void InitMeshPool( MeshPool* pool, MemoryArena* arena, sz size )
{
    // TODO Measure whether it's faster to just have a really big contiguous array for these
    pool->scratchVertices = BucketArray<TexturedVertex>( arena, 1024 );
    pool->scratchIndices = BucketArray<u32>( arena, 1024 );

    // Initialize empty sentinel
    pool->memorySentinel.prev = &pool->memorySentinel;
    pool->memorySentinel.next = &pool->memorySentinel;
    pool->memorySentinel.size = 0;
    pool->memorySentinel.flags = MemoryBlockFlags::None;

    // Insert empty block with the whole memory chunk
    InsertBlock( &pool->memorySentinel, size, PUSH_SIZE( arena, size ) );

    pool->meshCount = 0;
}

void ClearScratchBuffers( MeshPool* pool )
{
    pool->scratchVertices.Clear();
    pool->scratchIndices.Clear();
}

Mesh* AllocateMesh( MeshPool* pool, u32 vertexCount, u32 indexCount )
{
    sz vertexSize = sizeof(TexturedVertex) * vertexCount;
    sz indexSize = sizeof(u32) * indexCount;
    sz totalMeshSize = sizeof(Mesh) + vertexSize + indexSize;

    Mesh* result = nullptr;
    // FIXME This is wrong. Not accounting for the size of the MemoryBlock struct (which we shouldn't have to do anyway!)
    MemoryBlock* block = FindBlockForSize( &pool->memorySentinel, totalMeshSize );
    if( block )
    {
        // TODO Tune this based on the smallest mesh size we will typically have
        const sz blockSplitThreshold = 4096;
        result = (Mesh*)UseBlock( block, totalMeshSize, blockSplitThreshold );

        InitMesh( result );
        result->vertices = (TexturedVertex*)((u8*)result + sizeof(Mesh));
        result->indices = (u32*)((u8*)result->vertices + vertexSize);
        result->ownerPool = pool;

        pool->meshCount++;
    }
    else
    {
        // TODO Here we could evict meshes in a loop based on LRU
        // until we free the amount we need
        INVALID_CODE_PATH
    }

    return result;
}

Mesh* AllocateMeshFromScratchBuffers( MeshPool* pool )
{
    Mesh* result = AllocateMesh( pool, pool->scratchVertices.count,
                                 pool->scratchIndices.count );

    pool->scratchVertices.CopyTo( result->vertices );
    result->vertexCount = pool->scratchVertices.count;

    pool->scratchIndices.CopyTo( result->indices );
    result->indexCount = pool->scratchIndices.count;

    return result;
}

Mesh*
CopyMeshFromScratchBuffers( Mesh* mesh, MeshPool* pool )
{
    if( mesh->vertexCount != pool->scratchVertices.count ||
        mesh->indexCount != pool->scratchIndices.count )
    {
        mesh = AllocateMeshFromScratchBuffers( pool );
    }
    else
    {
        pool->scratchVertices.CopyTo( mesh->vertices );
        pool->scratchIndices.CopyTo( mesh->indices );
    }

    return mesh;
}

void ReleaseMesh( Mesh** mesh )
{
    // TODO Move the sentinel in MeshPool to a 'parent' MemoryPool and add a reference to that
    // in the MemoryBlock so we can remove the ownerPool thing!
    ReleaseBlockAt( *mesh, &(*mesh)->ownerPool->memorySentinel );
    (*mesh)->ownerPool->meshCount--;
    *mesh = nullptr;
}



///// QUADRATIC ERROR FUNCTIONS /////

union Mat4x4
{
    float	m[4][4];
    __m128	row[4];
};

static INLINE __m128 vec4_abs(const __m128& x)
{
	static const __m128 mask = _mm_set1_ps(-0.f);
	return _mm_andnot_ps(mask, x);
}

static INLINE float vec4_dot(const __m128& a, const __m128& b)
{
	// apparently _mm_dp_ps isn't well implemented in hardware so this "basic" version is faster
	__m128 mul = _mm_mul_ps(a, b);
	__m128 s0  = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
	__m128 add = _mm_add_ps(mul, s0);
	__m128 s1  = _mm_shuffle_ps(add, add, _MM_SHUFFLE(0, 1, 2, 3));
	__m128 res = _mm_add_ps(add, s1);
	return _mm_cvtss_f32(res);
}

static INLINE __m128 vec4_mul_m4x4(const __m128& a, const Mat4x4& B)
{
	__m128 result = _mm_mul_ps(_mm_shuffle_ps(a, a, 0x00), B.row[0]);
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0x55), B.row[1]));
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xaa), B.row[2]));
	result = _mm_add_ps(result, _mm_mul_ps(_mm_shuffle_ps(a, a, 0xff), B.row[3]));
	return result;
}

static void givens_coeffs_sym(__m128& c_result, __m128& s_result, const Mat4x4& vtav, const int a, const int b)
{
	__m128 simd_pp = _mm_set_ps( 0.f, vtav.row[a].m128_f32[a], vtav.row[a].m128_f32[a], vtav.row[a].m128_f32[a] );
	__m128 simd_pq = _mm_set_ps( 0.f, vtav.row[a].m128_f32[b], vtav.row[a].m128_f32[b], vtav.row[a].m128_f32[b] );
	__m128 simd_qq = _mm_set_ps( 0.f, vtav.row[b].m128_f32[b], vtav.row[b].m128_f32[b], vtav.row[b].m128_f32[b] );

	static const __m128 zeros = _mm_set1_ps(0.f);
	static const __m128 ones  = _mm_set1_ps(1.f);
	static const __m128 twos  = _mm_set1_ps(2.f);

	// tau = (a_qq - a_pp) / (2.f * a_pq);
	__m128 pq2 = _mm_mul_ps(simd_pq, twos);
	__m128 qq_sub_pp = _mm_sub_ps(simd_qq, simd_pp);
	__m128 tau = _mm_div_ps(qq_sub_pp, pq2);

	// stt = sqrt(1.f + tau * tau);
	__m128 tau_sq = _mm_mul_ps(tau, tau);
	__m128 tau_sq_1 = _mm_add_ps(tau_sq, ones);
	__m128 stt = _mm_sqrt_ps(tau_sq_1);
	
	// tan = 1.f / ((tau >= 0.f) ? (tau + stt) : (tau - stt));
	__m128 tan_gt = _mm_add_ps(tau, stt);
	__m128 tan_lt = _mm_sub_ps(tau, stt);
	__m128 tan_cmp = _mm_cmpge_ps(tau, zeros);
	__m128 tan_cmp_gt = _mm_and_ps(tan_cmp, tan_gt);
	__m128 tan_cmp_lt = _mm_andnot_ps(tan_cmp, tan_lt);
	__m128 tan_inv = _mm_or_ps(tan_cmp_gt, tan_cmp_lt);
	__m128 tan = _mm_div_ps(ones, tan_inv);

	// c = rsqrt(1.f + tan * tan);
	__m128 tan_sq = _mm_mul_ps(tan, tan);
	__m128 tan_sq_1 = _mm_add_ps(ones, tan_sq);
	__m128 c = _mm_rsqrt_ps(tan_sq_1);

	// s = tan * c;
	__m128 s = _mm_mul_ps(tan, c);

	// if pq == 0.0: c = 1.f, s = 0.f
	__m128 pq_cmp = _mm_cmpeq_ps(simd_pq, zeros);

	__m128 c_true = _mm_and_ps(pq_cmp, ones);
	__m128 c_false = _mm_andnot_ps(pq_cmp, c);
	c_result = _mm_or_ps(c_true, c_false);

	__m128 s_true = _mm_and_ps(pq_cmp, zeros);
	__m128 s_false = _mm_andnot_ps(pq_cmp, s);
	s_result = _mm_or_ps(s_true, s_false);
}

static void rotateq_xy(Mat4x4& vtav, const __m128& c, const __m128& s, const int a, const int b)
{
	__m128 u = _mm_set_ps( 0.f, vtav.row[a].m128_f32[a], vtav.row[a].m128_f32[a], vtav.row[a].m128_f32[a] );
	__m128 v = _mm_set_ps( 0.f, vtav.row[b].m128_f32[b], vtav.row[b].m128_f32[b], vtav.row[b].m128_f32[b] );
	__m128 A = _mm_set_ps( 0.f, vtav.row[a].m128_f32[b], vtav.row[a].m128_f32[b], vtav.row[a].m128_f32[b] );

	static const __m128 twos = _mm_set1_ps(2.f);

	__m128 cc = _mm_mul_ps(c, c);
	__m128 ss = _mm_mul_ps(s, s);
	
	// mx = 2.0 * c * s * A;
	__m128 c2 = _mm_mul_ps(twos, c);
	__m128 c2s = _mm_mul_ps(c2, s);
	__m128 mx = _mm_mul_ps(c2s, A);

	// x = cc * u - mx + ss * v;
	__m128 x0 = _mm_mul_ps(cc, u);
	__m128 x1 = _mm_sub_ps(x0, mx);
	__m128 x2 = _mm_mul_ps(ss, v);
	__m128 x  = _mm_add_ps(x1, x2);

	// y = ss * u + mx + cc * v;
	__m128 y0 = _mm_mul_ps(ss, u);
	__m128 y1 = _mm_add_ps(y0, mx);
	__m128 y2 = _mm_mul_ps(cc, v);
	__m128 y  = _mm_add_ps(y1, y2);

	vtav.row[a].m128_f32[a] = x.m128_f32[0];
	vtav.row[b].m128_f32[b] = y.m128_f32[0];
}

static void rotate_xy(Mat4x4& vtav, Mat4x4& v, float c, float s, const int a, const int b) 
{
    __m128 simd_u = _mm_set_ps( vtav.row[0].m128_f32[3-b], v.row[2].m128_f32[a], v.row[1].m128_f32[a], v.row[0].m128_f32[a] );
	__m128 simd_v = _mm_set_ps( vtav.row[1-a].m128_f32[2], v.row[2].m128_f32[b], v.row[1].m128_f32[b], v.row[0].m128_f32[b] );

	__m128 simd_c = _mm_load1_ps(&c);
	__m128 simd_s = _mm_load1_ps(&s);

	__m128 x0 = _mm_mul_ps(simd_c, simd_u);
	__m128 x1 = _mm_mul_ps(simd_s, simd_v);
	__m128 x = _mm_sub_ps(x0, x1);

	__m128 y0 = _mm_mul_ps(simd_s, simd_u);
	__m128 y1 = _mm_mul_ps(simd_c, simd_v);
	__m128 y = _mm_add_ps(y0, y1);

	v.row[0].m128_f32[a] = x.m128_f32[0];
	v.row[1].m128_f32[a] = x.m128_f32[1];
	v.row[2].m128_f32[a] = x.m128_f32[2];
	vtav.row[0].m128_f32[3-b] = x.m128_f32[3];

	v.row[0].m128_f32[b] = y.m128_f32[0];
	v.row[1].m128_f32[b] = y.m128_f32[1];
	v.row[2].m128_f32[b] = y.m128_f32[2];
	vtav.row[1-a].m128_f32[2] = y.m128_f32[3];

	vtav.row[a].m128_f32[b] = 0.f;
}


constexpr const int SVDNumSweeps = 5;
constexpr const float PseudoInverseThreshold = 0.001f;

// 'Classic' quadric error minimizer using SVD decomposition
// Taken from https://github.com/nickgildea/qef (just inlined most of it)
__m128 QEFMinimizePlanesClassic( const __m128* positions, const __m128* normals, const int count, float* error = nullptr )
{
	Mat4x4 ATA = {};
	__m128 ATb = _mm_set1_ps( 0.f );
	__m128 pointaccum = _mm_set1_ps( 0.f );

	for( int i = 0; i < count; i++ )
	{
		const __m128& p = positions[i];
		const __m128& n = normals[i];

		__m128 nX = _mm_mul_ps( _mm_shuffle_ps( n, n, _MM_SHUFFLE( 0, 0, 0, 0 ) ), n );
		__m128 nY = _mm_mul_ps( _mm_shuffle_ps( n, n, _MM_SHUFFLE( 1, 1, 1, 1 ) ), n );
		__m128 nZ = _mm_mul_ps( _mm_shuffle_ps( n, n, _MM_SHUFFLE( 2, 2, 2, 2 ) ), n );

		ATA.row[0] = _mm_add_ps( ATA.row[0], nX );
		ATA.row[1] = _mm_add_ps( ATA.row[1], nY );
		ATA.row[2] = _mm_add_ps( ATA.row[2], nZ );

		const float d = vec4_dot( p, n );
		__m128 x = _mm_set_ps( 0.f, d, d, d );
		x = _mm_mul_ps( x, n );
		ATb = _mm_add_ps( ATb, x );

		pointaccum = _mm_add_ps( pointaccum, p );
	}

	__declspec(align(16)) float x[4];
	_mm_store_ps( x, ATb );
	_mm_set_ps( 0.f, x[2], x[1], x[0] );

	const __m128 masspoint = _mm_div_ps( pointaccum, _mm_set1_ps( (float)count ) );

	__m128 p = _mm_mul_ps( _mm_shuffle_ps( masspoint, masspoint, 0x00 ), ATA.row[0] );
	p = _mm_add_ps( p, _mm_mul_ps( _mm_shuffle_ps( masspoint, masspoint, 0x55 ), ATA.row[1] ) );
	p = _mm_add_ps( p, _mm_mul_ps( _mm_shuffle_ps( masspoint, masspoint, 0xaa ), ATA.row[2] ) );
	p = _mm_add_ps( p, _mm_mul_ps( _mm_shuffle_ps( masspoint, masspoint, 0xff ), ATA.row[3] ) );
	p = _mm_sub_ps( ATb, p );

	Mat4x4 V;
	V.row[0] = _mm_set_ps( 0.f, 0.f, 0.f, 1.f );
	V.row[1] = _mm_set_ps( 0.f, 0.f, 1.f, 0.f );
	V.row[2] = _mm_set_ps( 0.f, 1.f, 0.f, 0.f );
	V.row[3] = _mm_set_ps( 0.f, 0.f, 0.f, 0.f );

	Mat4x4 ATAcopy = ATA;
	for( int i = 0; i < SVDNumSweeps; ++i )
	{
		__m128 c, s;

		if( ATA.row[0].m128_f32[1] != 0.f )
		{
			givens_coeffs_sym( c, s, ATA, 0, 1 );
			rotateq_xy( ATA, c, s, 0, 1 );
			rotate_xy( ATA, V, c.m128_f32[1], s.m128_f32[1], 0, 1 );
			ATA.row[0].m128_f32[1] = 0.f;
		}

		if( ATA.row[0].m128_f32[2] != 0.f )
		{
			givens_coeffs_sym( c, s, ATA, 0, 2 );
			rotateq_xy( ATA, c, s, 0, 2 );
			rotate_xy( ATA, V, c.m128_f32[1], s.m128_f32[1], 0, 2 );
			ATA.row[0].m128_f32[2] = 0.f;
		}

		if( ATA.row[1].m128_f32[2] != 0.f )
		{
			givens_coeffs_sym( c, s, ATA, 1, 2 );
			rotateq_xy( ATA, c, s, 1, 2 );
			rotate_xy( ATA, V, c.m128_f32[2], s.m128_f32[2], 1, 2 );
			ATA.row[1].m128_f32[2] = 0.f;
		}
	}

	__m128 sigma = _mm_set_ps( 0.f, ATA.row[2].m128_f32[2], ATA.row[1].m128_f32[1], ATA.row[0].m128_f32[0] );

	// A = UEV^T; U = A / (E*V^T)
	static const __m128 ones = _mm_set1_ps( 1.f );
	static const __m128 tol = _mm_set1_ps( PseudoInverseThreshold );

	__m128 abs_x = vec4_abs( sigma );
	__m128 one_over_x = _mm_div_ps( ones, sigma );
	__m128 abs_one_over_x = vec4_abs( one_over_x );
	__m128 min_abs = _mm_min_ps( abs_x, abs_one_over_x );
	__m128 cmp = _mm_cmpge_ps( min_abs, tol );
	__m128 invdet = _mm_and_ps( cmp, one_over_x );

	Mat4x4 m;
	m.row[0] = _mm_mul_ps( V.row[0], invdet );
	m.row[1] = _mm_mul_ps( V.row[1], invdet );
	m.row[2] = _mm_mul_ps( V.row[2], invdet );

	Mat4x4 Vinv = {};
	Vinv.row[0].m128_f32[0] = vec4_dot( m.row[0], V.row[0] );
	Vinv.row[0].m128_f32[1] = vec4_dot( m.row[1], V.row[0] );
	Vinv.row[0].m128_f32[2] = vec4_dot( m.row[2], V.row[0] );

	Vinv.row[1].m128_f32[0] = vec4_dot( m.row[0], V.row[1] );
	Vinv.row[1].m128_f32[1] = vec4_dot( m.row[1], V.row[1] );
	Vinv.row[1].m128_f32[2] = vec4_dot( m.row[2], V.row[1] );

	Vinv.row[2].m128_f32[0] = vec4_dot( m.row[0], V.row[2] );
	Vinv.row[2].m128_f32[1] = vec4_dot( m.row[1], V.row[2] );
	Vinv.row[2].m128_f32[2] = vec4_dot( m.row[2], V.row[2] );

	__m128 result = vec4_mul_m4x4( p, Vinv );

	if( error )
	{
		__m128 tmp = vec4_mul_m4x4( result, ATAcopy );
		tmp = _mm_sub_ps( ATb, tmp );

		*error = vec4_dot( tmp, tmp );
	}

	result = _mm_add_ps( result, masspoint );
	return result;
}


constexpr const int QEFMaxInputCount = 12;

v3 QEFMinimizePlanesClassic( v3 const* positions, v3 const* normals, int count, float* error = nullptr )
{
	ASSERT( count >= 2 || count <= QEFMaxInputCount );

	__m128 p[QEFMaxInputCount];
	__m128 n[QEFMaxInputCount];
	for( int i = 0; i < count; i++ )
	{
		v3 const& pos = positions[i];
		v3 const& nrm = normals[i];
		p[i] = _mm_set_ps( 1.f, pos.z, pos.y, pos.x );
		n[i] = _mm_set_ps( 0.f, nrm.z, nrm.y, nrm.x );
	}

	__m128 solved = QEFMinimizePlanesClassic( p, n, count, error );

	v3 result =
	{
		solved.m128_f32[0],
		solved.m128_f32[1],
		solved.m128_f32[2],
	};
	return result;
}



// New probabilistic quadric error minimizer
// Adapted from https://www.graphics.rwth-aachen.de/publication/03308/
// (inlined everything, replaced with plain types, got 100% speed increase!)
// TODO Estimate error
// TODO SIMD
v3 QEFMinimizePlanesProbabilistic( v3 const* points, v3 const* normals, int count, float stdDevP, float stdDevN )
{
	float A00 = 0.f;
	float A01 = 0.f;
	float A02 = 0.f;
	float A11 = 0.f;
	float A12 = 0.f;
	float A22 = 0.f;

	float b0 = 0.f;
	float b1 = 0.f;
	float b2 = 0.f;

	float c = 0.f;

	for( int i = 0; i < count; ++i )
	{
		v3 const& p = points[i];
		v3 const& n = normals[i];

		const float pn = p.x * n.x + p.y * n.y + p.z * n.z;

		const float nx = n.x;
		const float ny = n.y;
		const float nz = n.z;
		const float nxny = nx * ny;
		const float nxnz = nx * nz;
		const float nynz = ny * nz;
		const float sn2 = stdDevN * stdDevN;

		const v3 A0 = { nx * nx + sn2, nxny, nxnz };
		const v3 A1 = { nxny, ny * ny + sn2, nynz };
		const v3 A2 = { nxnz, nynz, nz * nz + sn2 };
		A00 += A0.x;
		A01 += A0.y;
		A02 += A0.z;
		A11 += A1.y;
		A12 += A1.z;
		A22 += A2.z;

		v3 const b = n * pn + p * sn2;
		b0 += b.x;
		b1 += b.y;
		b2 += b.z;

		const float sp2 = stdDevP * stdDevP;
		const float pp = p.x * p.x + p.y * p.y + p.z * p.z;
		const float nn = n.x * n.x + n.y * n.y + n.z * n.z;
		c += pn * pn + sn2 * pp + sp2 * nn + 3 * sp2 * sn2;
	}

	// Solving Ax = r with some common subexpressions precomputed
	float A00A12 = A00 * A12;
	float A01A22 = A01 * A22;
	float A11A22 = A11 * A22;
	float A02A12 = A02 * A12;
	float A02A11 = A02 * A11;

	float A01A12_A02A11 = A01 * A12 - A02A11;
	float A01A02_A00A12 = A01 * A02 - A00A12;
	float A02A12_A01A22 = A02A12 - A01A22;

	float denom = A00 * A11A22 + 2.f * A01 * A02A12 - A00A12 * A12 - A01A22 * A01 - A02A11 * A02;
    ASSERT( denom != 0.f );
	denom = 1.f / denom;
	float nom0 = b0 * (A11A22 - A12 * A12) + b1 * A02A12_A01A22 + b2 * A01A12_A02A11;
	float nom1 = b0 * A02A12_A01A22 + b1 * (A00 * A22 - A02 * A02) + b2 * A01A02_A00A12;
	float nom2 = b0 * A01A12_A02A11 + b1 * A01A02_A00A12 + b2 * (A00 * A11 - A01 * A01);

	v3 result = { nom0 * denom, nom1 * denom, nom2 * denom };
	return result;
}



///// CONTOURING /////

struct VertexCacheIndex
{
    // Offset to the cell entry 'owning' the corresponding edge
    v2i vCellOffset;
    // Which table, 0 - bottom, 1 - middle (vertical), 2 - top
    u16 cacheTableIndex;
    // Additional offset in table, for bottom & top only, 0 - X axis, 1 - Y axis
    u16 cacheTableOffset;
};

// This is just to allow forward-declaring internal arrays while still keeping them internal
// C++ is wonderful
namespace
{
    extern v3i cornerOffsets[8];
    extern u32 edgeVertexOffsets[12][2];
    extern VertexCacheIndex vertexCacheIndices[12];
    extern int triangleTable[][16];
}

void MarchCube( const v3& cellCornerWorldP, const v2i& gridCellP, v2u const& cellsPerAxis, r32 cellSizeMeters,
                IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices, BucketArray<u32>* indices,
                const bool interpolate /*= true*/ )
{
    TIMED_BLOCK;

    // Cache layers contain one sample per _edge_
    v2u layerStepsPerAxis = cellsPerAxis + V2uOne;

    // Construct case mask from 8 corner samples
    u32 caseIndex = 0;
    r32 cornerSamples[8];
    for( int i = 0; i < 8; ++i )
    {
        v3i cornerOffset = cornerOffsets[i];
        v3i layerP = V3i( gridCellP ) + cornerOffset;
        u32 layerOffset = layerP.y * layerStepsPerAxis.x + layerP.x;

        r32 sample = cornerOffset.z
            ? samplingCache->topLayerSamples[layerOffset]
            : samplingCache->bottomLayerSamples[layerOffset];

        if( sample >= 0 )
            caseIndex |= 1 << i;

        cornerSamples[i] = sample;
    }

    // Early out if entirely inside or outside
    if( caseIndex == 0 || caseIndex == 0xFF )
        return;

    u32* vertexCaches[3] =
    {
        samplingCache->bottomLayerVertexIndices,
        samplingCache->middleLayerVertexIndices,
        samplingCache->topLayerVertexIndices,
    };

    int caseVertex = 0;
    for( int i = 0; i < 5; ++i )
    {
        for( int tri = 0; tri < 3; ++tri )
        {
            int edgeCaseIndex = triangleTable[caseIndex][caseVertex];
            if( edgeCaseIndex == -1 )
                return;

            // First check if the vertex for this edge case is already in the cache
            VertexCacheIndex& idx = vertexCacheIndices[edgeCaseIndex];
            u32* vertexCache = vertexCaches[idx.cacheTableIndex];

            v2i layerP = gridCellP + idx.vCellOffset;
            u32 vertexCacheOffset = layerP.y * layerStepsPerAxis.x + layerP.x;
            if( idx.cacheTableIndex != 1)
            {
                // Top or bottom layers
                vertexCacheOffset = 2 * vertexCacheOffset + idx.cacheTableOffset;
            }

#if 1       // Use vertex cache (produces around 1/8th vertices)
            u32 cachedVertexIndex = vertexCache[vertexCacheOffset];
            if( cachedVertexIndex != U32MAX )
            {
                indices->Push( cachedVertexIndex );
            }
            else
#endif
            {
                // No cached vertex, so go on and create one
                u32 indexA = edgeVertexOffsets[edgeCaseIndex][0];   // Edge start
                u32 indexB = edgeVertexOffsets[edgeCaseIndex][1];   // Edge end

                v3 pA = cellCornerWorldP + V3( cornerOffsets[indexA] ) * cellSizeMeters;
                v3 pB = cellCornerWorldP + V3( cornerOffsets[indexB] ) * cellSizeMeters;

                v3 vPos = {};
                if( interpolate )
                {
                    // Interpolate along the edge
                    float sA = cornerSamples[indexA];
                    float sB = cornerSamples[indexB];

                    float diff = sA - sB;
                    // In case sampled function is the same, arbitrarily use midpoint
                    float t = AlmostEqual( diff, 0.f ) ? 0.5f : sA / diff;
                    vPos = pA + ((pB - pA) * t);
                }
                else
                {
                    // Non interpolated version (Discrete MC)
                    vPos = (pA + pB) / 2;
                }

                // Add new vertex to the mesh
                u32 newVertexIndex = vertices->count;
                indices->Push( newVertexIndex );
                TexturedVertex v = {};
                v.p = vPos;
                v.color = Pack01ToRGBA( 1, 1, 1, 1 );
                vertices->Push( v );

                // Cache it too!
                vertexCache[vertexCacheOffset] = newVertexIndex;
            }

            ++caseVertex;
        }
    }
}

Mesh*
MarchAreaFast( WorldCoords const& worldP, v3 const& areaSideMeters, r32 cellSizeMeters, IsoSurfaceFunc* sampleFunc,
               const void* samplingData, IsoSurfaceSamplingCache* samplingCache, MeshPool* meshPool, const bool interpolate = true )
{
    ClearScratchBuffers( meshPool );

    {
        TIMED_BLOCK;

        // TODO Should do ceil
        v2u cellsPerSliceAxis = V2u( areaSideMeters.xy / cellSizeMeters );
        u32 sliceCount = (u32)(areaSideMeters.z / cellSizeMeters);
        ASSERT( samplingCache->cellsPerAxis.x >= cellsPerSliceAxis.x && samplingCache->cellsPerAxis.y >= cellsPerSliceAxis.y );
        v3 halfSideMeters = areaSideMeters / 2;
        v3 cornerOffset = -halfSideMeters;

        v3 vXDelta = V3( cellSizeMeters, 0, 0 );
        v3 vYDelta = V3( 0, cellSizeMeters, 0 );

        v2u gridLinesPerAxis = cellsPerSliceAxis + V2uOne;
        bool firstSlice = true;

        WorldCoords p = worldP;

        // Iterate slices, we consider both the bottom and the top samples of the cubes on each pass
        for( u32 k = 0; k < sliceCount; ++k )
        {
            r32* bottomSamples = samplingCache->bottomLayerSamples;
            r32* topSamples = samplingCache->topLayerSamples;

            // Pre-sample top and bottom corners of cubes for each slice (actually only the top for all slices except the first one)
            // so we only sample one corner per cube instead of 8
            for( int n = 0; n < 2; ++n )
            {
                if( n == 0 && !firstSlice )
                    continue;

                r32* sampledLayer = n ? topSamples : bottomSamples;

                p.relativeP = worldP.relativeP + cornerOffset + V3i( 0, 0, k + n ) * cellSizeMeters;

                r32* sample = sampledLayer;
                // Iterate grid lines when sampling each layer, since we need to have samples at the extremes too
                for( u32 j = 0; j < gridLinesPerAxis.y; ++j )
                {
                    v3 pAtRowStart = p.relativeP;
                    for( u32 i = 0; i < gridLinesPerAxis.x; ++i )
                    {
                        *sample++ = sampleFunc( samplingData, p );
                        p.relativeP += vXDelta;
                    }
                    p.relativeP = pAtRowStart + vYDelta;
                }
            }

            // Keep a cache of already calculated vertices to eliminate duplication
            ClearVertexCaches( samplingCache, firstSlice );

            p.relativeP = worldP.relativeP + cornerOffset + V3i( 0, 0, k ) * cellSizeMeters;
            for( u32 j = 0; j < cellsPerSliceAxis.y; ++j )
            {
                v3 pAtRowStart = p.relativeP;
                for( u32 i = 0; i < cellsPerSliceAxis.x; ++i )
                {
                    MarchCube( p.relativeP, V2i( i, j ), cellsPerSliceAxis, cellSizeMeters, samplingCache,
                               &meshPool->scratchVertices, &meshPool->scratchIndices, interpolate );
                    p.relativeP += vXDelta;
                }
                p.relativeP = pAtRowStart + vYDelta;
            }

            firstSlice = false;
            SwapTopAndBottomLayers( samplingCache );
        }
    }

    // Write output mesh
    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    return result;
}



// TODO Clean up asserts
Mesh*
DCArea( WorldCoords const& worldP, v3 const& areaSideMeters, r32 cellSizeMeters, IsoSurfaceFunc* sampleFunc, const void* samplingData,
        MeshPool* meshPool, MemoryArena* tempArena, DCSettings const& settings )
{
    struct CellData
    {
        // NOTE If we clamped the final SDF field, say -1 to 1, we could probably get away with much smaller fixed-point numbers
        // 0, 1, 2 correspond to the edges aligned to X, Y, Z in each cell
        v3 edgeCrossingsP[3];
        v3 edgeCrossingsN[3];
        // v3 n; // NOTE Unused for now
        // Only one sample per cell (the 'max' corner of the aabb)
        r32 sampledValue;
        u32 vertexIndex;
    };

    // Relative to 'max' aabb point, also used to locate neighbour cells
    static const v3i dcCornerOffsets[8] =
    {
        V3i( -1, -1, -1 ),    // Bottom layer
        V3i(  0, -1, -1 ),
        V3i( -1,  0, -1 ),
        V3i(  0,  0, -1 ),
        V3i( -1, -1,  0 ),    // Top layer
        V3i(  0, -1,  0 ),
        V3i( -1,  0,  0 ),
        V3i(  0,  0,  0 ),
    };

    struct EdgeLocator
    {
        u32 cornerA;
        u32 cornerB;
        u32 neighbourIndex;
        u32 storeIndex;
    };

    static const EdgeLocator dcEdgeLocators[12] =
    {
        { 0, 1, 1, 0 },     // X - bottom layer
        { 1, 3, 3, 1 },     // Y
        { 3, 2, 3, 0 },     // X
        { 2, 0, 2, 1 },     // Y
        { 4, 5, 5, 0 },     // X - top layer
        { 5, 7, 7, 1 },     // Y
        { 7, 6, 7, 0 },     // X
        { 6, 4, 6, 1 },     // Y
        { 0, 4, 4, 2 },     // Z - middle
        { 1, 5, 5, 2 },     // Z
        { 3, 7, 7, 2 },     // Z
        { 2, 6, 6, 2 },     // Z
    };

    ClearScratchBuffers( meshPool );

    // One extra layer of cells in X,Y,Z to store the needed info
    v3u cellsPerAxis = V3uCeil( areaSideMeters / cellSizeMeters ) + V3uOne;

    Grid3D<CellData> cellData = Grid3D<CellData>( tempArena, cellsPerAxis, Temporary() );
    PZERO( cellData.data, cellsPerAxis.x * cellsPerAxis.y * cellsPerAxis.z * sizeof(CellData) );

    v3 halfSideMeters = areaSideMeters / 2;
    v3 minGridP = worldP.relativeP - halfSideMeters;
    WorldCoords p = worldP;

    aabb bounds = AABB( worldP.relativeP, areaSideMeters );

    const r32 delta = 0.01f;
    const r32 deltaInv = 1.f / (2.f * delta);

    for( u32 k = 0; k < cellsPerAxis.z; ++k )
    {
        for( u32 j = 0; j < cellsPerAxis.y; ++j )
        {
            for( u32 i = 0; i < cellsPerAxis.x; ++i )
            {
                p.relativeP = minGridP + V3( i, j, k ) * cellSizeMeters;

                u32 caseMask = 0;
                r32 cornerSamples[8] = {};

                // Build our bitmask using the samples from every corner
                for( int s = 0; s < 8; ++s )
                {
                    cornerSamples[s] = R32MAX;

                    // Skip if we're gonna move backwards in an axis but we're already at the border
                    if( i == 0 && (s & 0x1) == 0 )
                        continue;
                    if( j == 0 && (s & 0x2) == 0 )
                        continue;
                    if( k == 0 && (s & 0x4) == 0 )
                        continue;

                    r32 sample = R32NAN;
                    if( s == 7 )
                    {
                        // Sample our own
                        // (we sample an extra layer of cells ahead of the actual contoured area)
                        sample = sampleFunc( samplingData, p );
                        cellData( i, j, k ).sampledValue = sample;
                    }
                    else
                    {
                        v3u cellOffset = V3u( V3i( i, j, k ) + dcCornerOffsets[s] );
                        sample = cellData( cellOffset ).sampledValue;
                    }

                    if( Sign( sample ) )
                        caseMask |= 1 << s;
                    cornerSamples[s] = sample;
                }

                // Early out if entirely inside or outside
                if( caseMask == 0 || caseMask == 0xFF )
                    continue;

                v2u edges[12];
                v3 edgePoints[12];
                v3 edgeNormals[12];
                u32 pointCount = 0;

                // TODO As explained in http://www.inf.ufrgs.br/~comba/papers/thesis/diss-leonardo.pdf (4.2.2 & figure 4.4)
                // we only need to process 3 edges per cell
                // Find edge intersections or get them from neighbours
                for( int e = 0; e < 12; ++e )
                {
                    EdgeLocator const& locator = dcEdgeLocators[e];

                    u32 indexA = locator.cornerA;
                    u32 indexB = locator.cornerB;
                    r32 sA = cornerSamples[ indexA ];
                    r32 sB = cornerSamples[ indexB ];

                    if( Sign( sA ) != Sign( sB ) )
                    {
                        if( indexB == 7 || indexA == 7 )
                        {
                            // It's one of the edges stored in this cell, so find the intersection & normal
                            v3 pA = p.relativeP + V3( dcCornerOffsets[indexA] ) * cellSizeMeters;
                            v3 pB = p.relativeP + V3( dcCornerOffsets[indexB] ) * cellSizeMeters;
                            v3 edgeP = {};

                            if( settings.approximateEdgeIntersection )
                            {
                                // Just interpolate along the edge
                                r32 t = sA / (sA - sB);
                                Clamp01( t );
                                edgeP = Lerp( pA, pB, t );
                            }
                            else
                            {
                                // TODO Do a binary search along the edge
                                NOT_IMPLEMENTED;
                            }
                            cellData( i, j, k ).edgeCrossingsP[locator.storeIndex] = edgeP;
                            edgePoints[pointCount] = edgeP;

                            ASSERT( edgePoints[pointCount] != V3Zero );
                            ASSERT( Contains( bounds, edgePoints[pointCount] ) );

                            if( pointCount )
                                ASSERT( Distance( edgePoints[pointCount-1], edgePoints[pointCount] ) < 3.f );

                            ASSERT( (p.relativeP.x - 1 <= edgeP.x && edgeP.x <= p.relativeP.x && edgeP.y == p.relativeP.y && edgeP.z == p.relativeP.z)
                                || (p.relativeP.y - 1 <= edgeP.y && edgeP.y <= p.relativeP.y && edgeP.x == p.relativeP.x && edgeP.z == p.relativeP.z)
                                || (p.relativeP.z - 1 <= edgeP.z && edgeP.z <= p.relativeP.z && edgeP.x == p.relativeP.x && edgeP.y == p.relativeP.y) );

                            // Find normal vector by sampling near the intersection point we found
                            // TODO Mathematically this probably requires sampling on both sides of the point per axis,
                            // but we're doing it this way to save 3 samples per cell
                            // Gauge whether there's any noticeable difference in the result
                            p.relativeP = edgeP + V3( delta, 0, 0 );
                            r32 xDeltaSample = sampleFunc( samplingData, p );
                            p.relativeP = edgeP + V3( 0, delta, 0 );
                            r32 yDeltaSample = sampleFunc( samplingData, p );
                            p.relativeP = edgeP + V3( 0, 0, delta );
                            r32 zDeltaSample = sampleFunc( samplingData, p );

                            v3 normal = (V3( xDeltaSample, yDeltaSample, zDeltaSample ) - edgeP) * deltaInv;
                            Normalize( normal );
                            cellData( i, j, k ).edgeCrossingsN[locator.storeIndex] = normal;
                            edgeNormals[pointCount] = normal;
                        }
                        else
                        {
                            // This has already been calculated and stored in a neighbour cell so go get it
                            int neighbourIndex = locator.neighbourIndex;
                            ASSERT( neighbourIndex != 7 );

                            v3u neighbourCoords = V3u( V3i( i, j, k ) + dcCornerOffsets[neighbourIndex] );
                            edgePoints[pointCount] = cellData( neighbourCoords ).edgeCrossingsP[locator.storeIndex];
                            edgeNormals[pointCount] = cellData( neighbourCoords ).edgeCrossingsN[locator.storeIndex];

                            ASSERT( edgePoints[pointCount] != V3Zero );
                            ASSERT( Contains( bounds, edgePoints[pointCount] ) );

                            if( pointCount )
                                ASSERT( Distance( edgePoints[pointCount-1], edgePoints[pointCount] ) < 3.f );
                        }

                        edges[pointCount] = { indexA, indexB };
                        pointCount++;
                    }
                }

                ASSERT( pointCount );

                v3 cellVertex = V3Undefined;
                v3 massPoint = V3Zero;
                for( u32 pIndex = 0; pIndex < pointCount; ++pIndex )
                    massPoint += edgePoints[pIndex];
                massPoint /= (r32)pointCount;

                if( settings.approximateCellPoints )
                {
                    cellVertex = massPoint;

                    // TODO When merging cells in the octree, we still need to do a part of the QEF computation, as stated in "The Secret Sauce":
                    // "In addition to the data already stored for the mass point, we also store the dimension of the mass point"
                }
                else
                {
#if 0
                    cellVertex = QEFMinimizePlanesProbabilistic( edgePoints, edgeNormals, pointCount, 0.1f, 0.1f );
#else
                    cellVertex = QEFMinimizePlanesClassic( edgePoints, edgeNormals, pointCount );
#endif
                    ASSERT( Contains( bounds, cellVertex ) );





                    // If outside of the cell, initially just use the 'masspoint' (average) as in http://ngildea.blogspot.com/2014/11/implementing-dual-contouring.html
                    // (the final normal is also the average of the input normals)


                    // AFTER that, do the solve with constrains as explained in https://www.mattkeeter.com/projects/qef/
                    // (also check https://github.com/nickgildea/qef and https://github.com/BorisTheBrave/mc-dc/blob/a165b326849d8814fb03c963ad33a9faf6cc6dea/qef.py#L146)
                    // and see whether it makes any difference at all

                    // There are also SIMD and compute shader implementations in https://github.com/nickgildea/qef
                }

                //ASSERT( LengthSq( cellVertex ) < 105.f * 105.f );
                //ASSERT( Length( cellVertex ) > 80.f );

                u32 vertexIndex = meshPool->scratchVertices.count;
                TexturedVertex v = {};
                v.p = cellVertex;
                v.color = Pack01ToRGBA( 1, 1, 1, 1 );
                meshPool->scratchVertices.Push( v );

                cellData( i, j, k ).vertexIndex = vertexIndex;
#if 0
                v3 avgNormal = V3Zero;
                for( u32 p = 0; p < pointCount; ++p )
                    avgNormal += edgeNormals[p];
                avgNormal /= pointCount;
                cellData( i, j, k ).n = avgNormal;
#endif


                // Now we look 'backwards' and create at most 3 quads corresponding to the edges that include the 'min' corner instead,
                // as we know those vertices will be ready by now
                static const u32 edgesProducingPolys[3][2] =
                {
                    { 0, 1 },
                    { 0, 2 },
                    { 0, 4 },
                };

                for( u32 e = 0; e < 3; ++e )
                {
                    r32 sA = cornerSamples[ edgesProducingPolys[e][0] ];
                    r32 sB = cornerSamples[ edgesProducingPolys[e][1] ];

                    if( Sign( sA ) != Sign( sB ) )
                    {
                        switch( e )
                        {
                        case 0:     // Normal aligned to +/- X
                        {
                            if( sA < sB )
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k-1 ).vertexIndex ); 
                            }
                            else
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k-1 ).vertexIndex ); 
                            }
                        } break;
                        case 1:     // Normal aligned to +/- Y
                        {
                            if( sA < sB )
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k-1 ).vertexIndex );
                            }
                            else
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i, j, k-1 ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k-1 ).vertexIndex );
                            }
                        } break;
                        case 2:     // Normal aligned to +/- Z
                        {
                            if( sA < sB )
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j-1, k).vertexIndex );
                            }
                            else
                            {
                                meshPool->scratchIndices.Push( vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );

                                meshPool->scratchIndices.Push( cellData( i-1, j, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i, j-1, k ).vertexIndex );
                                meshPool->scratchIndices.Push( cellData( i-1, j-1, k).vertexIndex );
                            }
                        } break;
                        }
                    }
                }
            }
        }
    }

    // Write output mesh
    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    return result;
}



struct Metaball
{
    v3 pCenter;
    r32 radiusMeters;
};

ISO_SURFACE_FUNC(SampleMetaballs)
{
    const Array<Metaball>& balls = *(const Array<Metaball>*)samplingData;

    r32 minValue = R32MAX;
    for( u32 i = 0; i < balls.maxCount; ++i )
    {
        r32 value = DistanceSq( balls[i].pCenter, worldP.relativeP ) - balls[i].radiusMeters;
        if( value < minValue )
            minValue = value;
    }

    return minValue;
}

void
TestMetaballs( float areaSideMeters, float cellSizeMeters, float elapsedT, IsoSurfaceSamplingCache* samplingCache,
               MeshPool* meshPool, RenderCommands *renderCommands )
{
    persistent ARRAY(Metaball, 10, balls);

    if( balls[0].radiusMeters == 0 )
    {
        for( u32 i = 0; i < balls.maxCount; ++i )
        {
            //r32 x = RandomRange( -halfSideMeters, halfSideMeters );
            //r32 y = RandomRange( -halfSideMeters, halfSideMeters );
            //r32 z = RandomRange( -halfSideMeters, halfSideMeters );
            r32 r = RandomRangeR32( 1.0f, 5.0f );
            balls.Push( { V3Zero, r } );
        }
    }

    // Update positions
    for( u32 i = 0; i < balls.count; ++i )
    {
        Metaball& ball = balls[i];
        r32 x = (i+1) / 2.0f * Cos( elapsedT - i );
        r32 y = (i+2) / 2.0f * Sin( elapsedT + i );
        r32 z = (i+3) / 2.0f * Sin( elapsedT - i );

        ball.pCenter = { x, y, z };
    }
    
    // Update mesh by sampling our cubic area centered at origin
    Mesh* metaMesh = MarchAreaFast( { V3Zero, V3iZero }, V3( areaSideMeters ), cellSizeMeters,
                                    SampleMetaballs, &balls,
                                    samplingCache, meshPool );

    RenderMesh( *metaMesh, renderCommands );
}

ISO_SURFACE_FUNC( TorusSurfaceFunc )
{
    // NOTE We're axis aligned for now, so just translate
    v3 invWorldP = worldP.relativeP - V3Zero;

    r32 result = SDFTorus( invWorldP, 70, 30 );
    return result;
}

ISO_SURFACE_FUNC( BoxSurfaceFunc )
{
    // NOTE We're axis aligned for now, so just translate
    v3 invWorldP = worldP.relativeP - V3Zero;

    r32 result = SDFBox( invWorldP, { 70, 70, 70 } );
    return result;
}






///// MESH SIMPLIFICATION /////

internal inline r64
VertexError( const m4Symmetric& q, r64 x, r64 y, r64 z )
{
    // Error between vertex and quadric
    r64 result = q.e[0]*x*x + 2*q.e[1]*x*y + 2*q.e[2]*x*z + 2*q.e[3]*x
               + q.e[4]*y*y + 2*q.e[5]*y*z + 2*q.e[6]*y
               + q.e[7]*z*z + 2*q.e[8]*z
               + q.e[9];
    return result;
}

internal r64
CalculateError( FQSMesh* mesh, u32 v1Idx, u32 v2Idx, v3* result )
{
    FQSVertex& v1 = mesh->vertices[v1Idx];
    FQSVertex& v2 = mesh->vertices[v2Idx];

    // Compute interpolated vertex
    m4Symmetric q = v1.q + v2.q;
    bool border = v1.border && v2.border;
    r64 error = 0;
    r64 det = Determinant3x3( q, 0, 1, 2, 1, 4, 5, 2, 5, 7 );

    // TODO Epsilon?
    if( det != 0 && !border )
    {
        // Invertible
        result->x = (r32)(-1 / det * Determinant3x3( q, 1, 2, 3, 4, 5, 6, 5, 7, 8 ));
        result->y = (r32)( 1 / det * Determinant3x3( q, 0, 2, 3, 1, 5, 6, 2, 7, 8 ));
        result->z = (r32)(-1 / det * Determinant3x3( q, 0, 1, 3, 1, 4, 6, 2, 5, 8 ));
        error = VertexError( q, result->x, result->y, result->z );
    }
    else
    {
        // Try to find best result
        v3 p1 = v1.p;
        v3 p2 = v2.p;
        v3 p3 = (p1 + p2) / 2;
        r64 error1 = VertexError( q, p1.x, p1.y, p1.z );
        r64 error2 = VertexError( q, p2.x, p2.y, p2.z );
        r64 error3 = VertexError( q, p3.x, p3.y, p3.z );
        error = Min( error1, Min( error2, error3 ) );

        if( error1 == error )
            *result = p1;
        else if( error2 == error )
            *result = p2;
        else if( error3 == error )
            *result = p3;
    }

    return error;
}

internal void
UpdateMesh( FQSMesh* mesh, Array<FQSVertexRef>* refs, u32 iteration, const TemporaryMemory& tmpMemory )
{
    if( iteration > 0 )
    {
        // Compact tri array
        u32 dst = 0;
        for( u32 i = 0; i < mesh->triangles.count; ++i )
        {
            if( !mesh->triangles[i].deleted )
            {
                // FIXME Optimize this since probably the first N tris won't be deleted
                // and this copies them into themselves
                // Also, is this really necessary at all?
                mesh->triangles[dst++] = mesh->triangles[i];
            }
        }
        mesh->triangles.count = dst;
    }

    // Init quadrics plane & edge errors
    // Required at the first iteration. Not required later, but could improve the result for closed meshes
    if( iteration == 0 )
    {
        for( u32 i = 0; i < mesh->vertices.count; ++i )
            mesh->vertices[i].q = M4SymmetricZero;

        for( u32 i = 0; i < mesh->triangles.count; ++i )
        {
            FQSTriangle& tri = mesh->triangles[i];
            v3 n;
            v3 p[3] =
            {
                mesh->vertices[tri.v[0]].p,
                mesh->vertices[tri.v[1]].p,
                mesh->vertices[tri.v[2]].p
            };

            n = Normalized( Cross( p[1] - p[0], p[2] - p[0] ) );
            tri.n = n;

            for( int j = 0; j < 3; ++j )
            {
                mesh->vertices[tri.v[j]].q += M4Symmetric( n.x, n.y, n.z, -Dot( n, p[0] ) );
            }
        }

        for( u32 i = 0; i < mesh->triangles.count; ++i )
        {
            FQSTriangle& tri = mesh->triangles[i];
            v3 p;

            for( int j = 0; j < 3; ++j )
                tri.error[j] = CalculateError( mesh, tri.v[j], tri.v[(j+1)%3], &p );

            tri.error[3] = Min( tri.error[0], Min( tri.error[1], tri.error[2] ) );
        }
    }

    // Rebuild refs list
    for( u32 i = 0; i < mesh->vertices.count; ++i )
    {
        mesh->vertices[i].refStart = 0;
        mesh->vertices[i].refCount = 0;
    }
    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        FQSTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
            mesh->vertices[tri.v[j]].refCount++;
    }

    u32 refStart = 0;
    for( u32 i = 0; i < mesh->vertices.count; ++i )
    {
        FQSVertex& v = mesh->vertices[i];
        v.refStart = refStart;
        refStart += v.refCount;
        v.refCount = 0;
    }

    refs->count = mesh->triangles.count * 3;
    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        FQSTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
        {
            FQSVertex& v = mesh->vertices[tri.v[j]];
            FQSVertexRef& r = (*refs)[v.refStart + v.refCount];

            r.tId = i;
            r.tVertex = j;
            v.refCount++;
        }
    }

    // Identify boundary vertices
    if( iteration == 0 )
    {
        Array<u32> vCount( tmpMemory.arena, 0, 1000 );
        Array<u32> vIds( tmpMemory.arena, 0, 1000 );

        for( u32 i = 0; i < mesh->vertices.count; ++i )
            mesh->vertices[i].border = false;
        
        for( u32 i = 0; i < mesh->vertices.count; ++i )
        {
            vCount.count = 0;
            vIds.count = 0;

            FQSVertex& v = mesh->vertices[i];

            for( u32 j = 0; j < v.refCount; ++j )
            {
                u32 tId = (*refs)[v.refStart + j].tId;
                FQSTriangle& tri = mesh->triangles[tId];

                for( int k = 0; k < 3; ++k )
                {
                    u32 ofs = 0;
                    u32 id = tri.v[k];
                    while( ofs < vCount.count )
                    {
                        if( vIds[ofs] == id )
                            break;
                        ofs++;
                    }

                    if( ofs == vCount.count )
                    {
                        vCount.Push( 1 );
                        vIds.Push( id );
                    }
                    else
                        vCount[ofs]++;
                }
            }

            for( u32 j = 0; j < vCount.count; ++j )
            {
                if( vCount[j] == 1 )
                    mesh->vertices[vIds[j]].border = true;
            }
        }
    }
}

internal bool
Flipped( const FQSMesh& mesh, const Array<FQSVertexRef>& refs,
         const v3& p, u32 i0, u32 i1, const FQSVertex& v0, const FQSVertex& v1, Array<bool>* deleted )
{
    for( u32 k = 0; k < v0.refCount; ++k )
    {
        const FQSTriangle& tri = mesh.triangles[refs[v0.refStart + k].tId];
        if( tri.deleted )
            continue;

        u32 s = refs[v0.refStart + k].tVertex;
        u32 id1 = tri.v[(s+1)%3];
        u32 id2 = tri.v[(s+2)%3];

        if( id1 == i1 || id2 == i1 )    // delete?
        {
            (*deleted)[k] = true;
            continue;
        }

        v3 d1 = Normalized( mesh.vertices[id1].p - p );
        v3 d2 = Normalized( mesh.vertices[id2].p - p );
        if( Abs( Dot( d1, d2 ) ) > 0.999f )
            return true;

        v3 n = Normalized( Cross( d1, d2 ) );
        (*deleted)[k] = false;

        if( Dot( n, tri.n ) < 0.2f )
            return true;
    }

    return false;
}

// Update triangle connections and edge error after an edge is collapsed
internal void
UpdateTriangles( u32 i0, const FQSVertex& v, const Array<bool>& deleted,
                 FQSMesh* mesh, Array<FQSVertexRef>* refs, u32* deleteTriangleCount )
{
    v3 p;
    for( u32 k = 0; k < v.refCount; ++k )
    {
        FQSVertexRef& ref = (*refs)[v.refStart + k];
        FQSTriangle& tri = mesh->triangles[ref.tId];

        if( tri.deleted )
            continue;
        if( deleted[k] )
        {
            tri.deleted = true;
            (*deleteTriangleCount)++;
            continue;
        }

        tri.v[ref.tVertex] = i0;
        tri.dirty = true;
        tri.error[0] = CalculateError( mesh, tri.v[0], tri.v[1], &p );
        tri.error[1] = CalculateError( mesh, tri.v[1], tri.v[2], &p );
        tri.error[2] = CalculateError( mesh, tri.v[2], tri.v[0], &p );
        tri.error[3] = Min( tri.error[0], Min( tri.error[1], tri.error[2] ) );
        refs->Push( ref );
    }
}

internal void
CompactMesh( FQSMesh* mesh )
{
    u32 dst = 0;

    for( u32 i = 0; i < mesh->vertices.count; ++i )
        mesh->vertices[i].refCount = 0;

    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        FQSTriangle& tri = mesh->triangles[i];
        if( !tri.deleted )
        {
            mesh->triangles[dst++] = tri;
            for( int j = 0; j < 3; ++j )
                mesh->vertices[tri.v[j]].refCount = 1;
        }
    }
    mesh->triangles.count = dst;
    dst = 0;

    for( u32 i = 0; i < mesh->vertices.count; ++i )
    {
        FQSVertex& v = mesh->vertices[i];
        if( v.refCount )
        {
            v.refStart = dst;
            mesh->vertices[dst].p = v.p;
            dst++;
        }
    }

    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        FQSTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
            tri.v[j] = mesh->vertices[tri.v[j]].refStart;
    }
    mesh->vertices.count = dst;
}

// Taken from https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
void FastQuadricSimplify( FQSMesh* mesh, u32 targetTriCount, const TemporaryMemory& tmpMemory,
                          r32 agressiveness = 7 )
{
    Array<FQSVertexRef> refs( tmpMemory.arena, 0, 500000 );

    u32 triangleCount = mesh->triangles.count;
    u32 deletedTriangleCount = 0;

    for( u32 i = 0; i < triangleCount; ++i )
        mesh->triangles[i].deleted = false;

    for( int iteration = 0; iteration < 100; ++iteration )
    {
        if( triangleCount - deletedTriangleCount <= targetTriCount )
            break;

        // Update mesh every few cycles (including first time through)
        if( iteration % 5 == 0 )
            UpdateMesh( mesh, &refs, iteration, tmpMemory );

        for( u32 i = 0; i < mesh->triangles.count; ++i )
            mesh->triangles[i].dirty = false;

        // All triangles with edges below the threshold will be removed
        // NOTE The following numbers usually work. Adjust as needed
        r64 threshold = 0.000000001 * PowR64( r64(iteration + 3), agressiveness );

        if( iteration % 5 == 0 )
            LOG( "Iteration %d - tris %d  threshold %g", iteration, triangleCount - deletedTriangleCount, threshold );

        for( u32 i = 0; i < mesh->triangles.count; ++i )
        {
            FQSTriangle& tri = mesh->triangles[i];
            if( tri.error[3] > threshold || tri.deleted || tri.dirty )
                continue;

            for( int j = 0; j < 3; ++j )
            {
                if( tri.error[j] < threshold )
                {
                    Array<bool> deleted0( tmpMemory.arena, 0, 1000 );
                    Array<bool> deleted1( tmpMemory.arena, 0, 1000 );

                    u32 i0 = tri.v[j];          FQSVertex& v0 = mesh->vertices[i0];
                    u32 i1 = tri.v[(j+1)%3];    FQSVertex& v1 = mesh->vertices[i1];

                    if( v0.border != v1.border )
                        continue;

                    // Compute vertex to collapse to
                    v3 p;
                    CalculateError( mesh, i0, i1, &p );

                    deleted0.count = v0.refCount;
					deleted1.count = v1.refCount;

					// Don't remove if flipped
					if( Flipped( *mesh, refs, p, i0, i1, v0, v1, &deleted0 ) )
					    continue;
					if( Flipped( *mesh, refs, p, i1, i0, v1, v0, &deleted1 ) )
					    continue;

                    v0.p = p;
                    v0.q = v1.q + v0.q;

                    u32 refStart = refs.count;
                    UpdateTriangles( i0, v0, deleted0, mesh, &refs, &deletedTriangleCount );
                    UpdateTriangles( i0, v1, deleted1, mesh, &refs, &deletedTriangleCount );
                    u32 refCount = refs.count - refStart;

                    if( refCount <= v0.refCount )
                    {
                        // Save ram (sic) !?
                        if( refCount )
                            memcpy( &refs[v0.refStart], &refs[refStart], refCount * sizeof(FQSVertexRef) );
                    }
                    else
                        v0.refStart = refStart;

                    v0.refCount = refCount;
                    break;
                }
            }

            if( triangleCount - deletedTriangleCount <= targetTriCount )
                break;
        }
    }

    CompactMesh( mesh );
}

///// CONVERSION TO 'SAMPLED' MESHES /////

struct Hit
{
    v2i gridCoords;
    r32 hitCoord;
};

internal void
AddSorted( const Hit& hit, Array<Hit>* result )
{
    Array<Hit>& out = *result;
    out.PushEmpty();

    u32 i = out.count - 1;
    for( ; i > 0; --i )
    {
        if( out[i-1].hitCoord < hit.hitCoord )
        {
            out[i] = hit;
            break;
        }
        else
        {
            out[i] = out[i-1];
        }
    }

    if( i == 0 )
        out[0] = hit;
}

internal void
FilterHits( const Array<Hit>& hits, const v2i& gridCoords, Array<Hit>* result )
{
    result->Clear();
    for( u32 i = 0; i < hits.count; ++i )
    {
        if( hits[i].gridCoords == gridCoords )
            AddSorted( hits[i], result );
    }
}

Mesh* ConvertToIsoSurfaceMesh( const Mesh& sourceMesh, r32 drawingDistance, u32 displayedLayer, IsoSurfaceSamplingCache* samplingCache,
                               MeshPool* meshPool, const TemporaryMemory& tmpMemory, RenderCommands* renderCommands )
{
    // Make bounds same length on all axes
    v3 centerP = Center( sourceMesh.bounds );
    v3 extents = Extents( sourceMesh.bounds );
    r32 cubeSide = Max( extents.x, Max( extents.y, extents.z ) );

    aabb box = AABB( centerP, cubeSide );
    v3 const &gridOriginP = box.min;

    ASSERT( samplingCache->cellsPerAxis.x == samplingCache->cellsPerAxis.y );
    u32 cellsPerAxis = samplingCache->cellsPerAxis.x;
    u32 gridLinesPerAxis = cellsPerAxis + 1;
    r32 step = cubeSide / cellsPerAxis;

    // We store all intersections with grid rays in a hashtable where encoded integer coords are the hash
    // For example, for X rays, the Y|Z coords are the hash, for Y rays, the X|Z coords, etc.
    // NOTE This can be heavily compressed if needed by using a more compact hash, since most entries will be empty anyway
    u32 rayCount = gridLinesPerAxis * gridLinesPerAxis;       // Must be power of 2
    Array<Hit> gridHits( tmpMemory.arena, 0, 100000 );

    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

    // Get all triangles and find their bounds
    u32 triangleCount = sourceMesh.indexCount / 3;
    ASSERT( triangleCount * 3 == sourceMesh.indexCount );

    u32 vertIndex = 0;
    for( u32 i = 0; i < triangleCount; ++i )
    {
        TexturedVertex& v0 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        TexturedVertex& v1 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        TexturedVertex& v2 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        tri t = { v0.p, v1.p, v2.p };

        aabb triBounds =
        {
            {
                Min( v0.p.x, Min( v1.p.x, v2.p.x ) ),
                Min( v0.p.y, Min( v1.p.y, v2.p.y ) ),
                Min( v0.p.z, Min( v1.p.z, v2.p.z ) ),
            },
            {
                Max( v0.p.x, Max( v1.p.x, v2.p.x ) ),
                Max( v0.p.y, Max( v1.p.y, v2.p.y ) ),
                Max( v0.p.z, Max( v1.p.z, v2.p.z ) ),
            }
        };

        // Test each tri for intersection against all marching grid rays (early out using bounds first)
        // (this could be accelerated easily by using a binary search on grid coords)
        
        // Y rays
        for( u32 x = 0; x < gridLinesPerAxis; ++x )
        {
            r32 xGrid = box.min.x + x * step;

            if( triBounds.max.x < xGrid )
                break;
            if( xGrid >= triBounds.min.x )
            {
                for( u32 z = 0; z < gridLinesPerAxis; ++z )
                {
                    r32 zGrid = box.min.z + z * step;

                    if( triBounds.max.z < zGrid )
                        break;
                    if( zGrid >= triBounds.min.z )
                    {
                        ray r = { { xGrid, box.min.y, zGrid }, { 0, 1, 0 } };
                        v3 pI = V3Zero;

                        // NOTE Rays intersecting at the very edges of tris can be a problem at finer resolutions
                        // (not easily solved even when adding a manual tolerance)
                        if( Intersects( r, t, &pI ) )
                        {
                            // Store intersection coords relative to grid
                            r32 hitCoord = (pI - gridOriginP).y;
                            gridHits.Push( { V2i( x, z ), hitCoord } );
                        }
                    }
                }
            }
        }
    }

    ClearScratchBuffers( meshPool );
    bool firstLayer = true;

    for( u32 k = 0; k < cellsPerAxis; ++k )
    {
        r32* bottomSamples = samplingCache->bottomLayerSamples;
        r32* topSamples = samplingCache->topLayerSamples;

        // Pre-sample top and bottom slices of values for each layer
        // so we only sample one corner per cube instead of 8
        for( int n = 0; n < 2; ++n )
        {
            if( n == 0 && !firstLayer )
                continue;

            r32* sampledLayer = n ? topSamples : bottomSamples;
            r32* sample = sampledLayer;
            for( u32 i = 0; i < gridLinesPerAxis; ++i )
            {
                v2i vIK = n == 0 ? V2i( i, k ) : V2i( i, k+1 );

                // FIXME
                ARRAY(Hit, 10, rayHits);
                FilterHits( gridHits, vIK, &rayHits );

                // This usually signals a tolerance error in Intersects() (or a bogus mesh, of course)
                ASSERT( (rayHits.count & 0x1) == 0 );

                u32 h = 0;
                int value = 1;
                for( u32 j = 0; j < gridLinesPerAxis; ++j )
                {
                    r32 yGrid = j * step;
                    while( h < rayHits.count && rayHits[h].hitCoord < yGrid )
                    {
                        value *= -1;
                        h++;
                    }

                    *sample++ = (r32)value;
                }
            }
        }

        // Keep a cache of already calculated vertices to eliminate duplication
        ClearVertexCaches( samplingCache, firstLayer );

        for( u32 i = 0; i < cellsPerAxis; ++i )
        {
            for( u32 j = 0; j < cellsPerAxis; ++j )
            {
                v3 p = gridOriginP + V3i( i, j, k ) * step;

#if 1
                if( displayedLayer == k )
                {
                    u32 color = Pack01ToRGBA( 0, 1, 0, 1 );
                    r32 value = samplingCache->bottomLayerSamples[i * gridLinesPerAxis + j];
                    //if( value < 0 )
                        //RenderBoundsAt( p, 0.005f * drawingDistance, color, renderCommands );

                    value = samplingCache->topLayerSamples[i * gridLinesPerAxis + j];
                    if( value < 0 )
                        RenderBoundsAt( p + V3( 0, 0, step ), 0.005f * drawingDistance, color, renderCommands );
                }
#endif

                MarchCube( p, V2i( i, j ), V2u( cellsPerAxis ), step, samplingCache,
                           &meshPool->scratchVertices, &meshPool->scratchIndices );
            }
        }

        firstLayer = false;
        SwapTopAndBottomLayers( samplingCache );
    }

    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    result->bounds = box;
    return result;
}


///// MESH GENERATION /////

#if 0

ISO_SURFACE_FUNC(SampleCuboid)
{
    MeshGeneratorPathData* path = (MeshGeneratorPathData*)samplingData;
    v3 vRight = GetBasisX( path->basis );
    v3 vForward = GetBasisY( path->basis );
    v3 vUp = GetBasisZ( path->basis );

    // Calc distance to both aligned planes along dir
    // NOTE Max() means 'intersection'
    r32 dUp = Max( Dot( vUp, p ), Dot( -vUp, p ) ) - path->thicknessSq;
    r32 dRight = Max( Dot( vRight, p ), Dot( -vRight, p ) ) - path->thicknessSq;
 
    r32 result = Max( dUp, dRight );

    // Turns
    if( path->nextBasis )
    {
        // Cut with plane across motion dir
        // NOTE For now we're gonna assume that all turns take place at the area's center point
        r32 d = 0; // path->areaSideMeters/2 - path->distanceToNextTurn;
        result += Clamp0( Dot( vForward, p ) + d );

        vRight = GetBasisX( *path->nextBasis );
        vForward = GetBasisY( *path->nextBasis );
        vUp = GetBasisZ( *path->nextBasis );

        dUp = Max( Dot( vUp, p ), Dot( -vUp, p ) ) - path->thicknessSq;
        dRight = Max( Dot( vRight, p ), Dot( -vRight, p ) ) - path->thicknessSq;

        r32 newResult = Max( dUp, dRight );
        // Cut with plane across motion dir (backwards)
        // NOTE For now we're gonna assume that all turns take place at the area's center point
        d = 0; // path->areaSideMeters/2 - path->distanceToNextTurn;
        newResult += Clamp0( Dot( -vForward, p ) + d );

        // NOTE Min() means 'union'
        result = Min( result, newResult );
    }

    //Forks
    if( path->nextFork )
    {
        vRight = GetBasisX( path->nextFork->basis );
        vForward = GetBasisY( path->nextFork->basis );
        vUp = GetBasisZ( path->nextFork->basis );

        dUp = Max( Dot( vUp, p ), Dot( -vUp, p ) ) - path->thicknessSq;
        dRight = Max( Dot( vRight, p ), Dot( -vRight, p ) ) - path->thicknessSq;

        r32 newResult = Max( dUp, dRight );
        // Cut with plane across motion dir (bakcwards)
        // NOTE For now we're gonna assume that all forks take place at the area's center point
        r32 d = 0; // path->areaSideMeters/2 - path->distanceToNextFork;
        newResult += Clamp0( Dot( -vForward, p ) + d );

        // NOTE Min() means 'union'
        result = Min( result, newResult );
    }

    // FIXME Use an offset that's in proportion to step resolution (add resolution to path struct?)
    return result + 0.1f;
}

internal r32
SampleCylinder( const void* samplingData, const v3& p )
{
    MeshGeneratorPathData* path = (MeshGeneratorPathData*)samplingData;
    v3 vForward = GetBasisY( path->basis );

    v3 vP = p - path->pCenter;
    // l is the length of the projection of vP along the director line
    r32 l = Dot( vForward, vP );
    // Translate p the previous distance along vDir (backwards) to align it with pCenter
    v3 pPrime = p - vForward * l;

    r32 result = DistanceSq( path->pCenter, pPrime ) - path->thicknessSq;
    return result;
}

u32
GenerateOnePathStep( MeshGeneratorPathData* path, r32 resolutionMeters, bool advancePosition,
                     IsoSurfaceSamplingCache* samplingCache,
                     MeshPool* meshPool, Mesh** outMesh, MeshGeneratorPathData* nextFork )
{
    bool turnInThisStep = path->distanceToNextTurn < path->areaSideMeters;
    bool forkInThisStep = path->distanceToNextFork < path->areaSideMeters;

    r32 pi2 = PI / 2;
    m4 rotations[4] =
    {
        // Expressed as if +Y == 'forward'
        M4ZRotation(  pi2 ),
        M4ZRotation( -pi2 ),
        M4XRotation(  pi2 ),
        M4XRotation( -pi2 ),
    };
    u32 random = RandomU32();
    u32 rotIndex = random & 0x3;

    path->nextBasis = nullptr;
    path->nextFork = nullptr;

    m4 nextBasis = {};
    if( turnInThisStep )
    {
        m4& rot = rotations[rotIndex];

        // Create a new basis by randomly rotating the current one, and pass it to the next sampler
        // We're actually rotating the 'rotation' itself by our current basis
        nextBasis = path->basis * rot;  // NOT rot * path->basis
        path->nextBasis = &nextBasis;
    }

    if( forkInThisStep )
    {
        if( turnInThisStep )
            rotations[rotIndex] = M4Identity;

        rotIndex = (random >> 2) & 0x3;
        m4& rot = rotations[rotIndex];

        *nextFork = *path;
        nextFork->basis = path->basis * rot;
        nextFork->nextBasis = nullptr;
        path->nextFork = nextFork;
    }

    *outMesh = MarchAreaFast( V3Zero, path->areaSideMeters, resolutionMeters,
                              SampleCuboid, path, samplingCache, meshPool );
    (*outMesh)->mTransform = M4Translation( path->pCenter );

    // Advance to next chunk
    if( advancePosition )
    {
        path->distanceToNextTurn -= path->areaSideMeters;
        path->distanceToNextFork -= path->areaSideMeters;
        if( turnInThisStep )
        {
            path->basis = nextBasis;
            path->distanceToNextTurn = RandomRangeR32( path->minDistanceToTurn, path->maxDistanceToTurn );
        }
        if( forkInThisStep )
        {
            path->distanceToNextFork = RandomRangeR32( path->minDistanceToFork, path->maxDistanceToFork );
        }

        v3 vForward = GetBasisY( path->basis );
        path->pCenter += vForward * (path->areaSideMeters + 0.1f);
    }

    return forkInThisStep ? 1 : 0;
}

#endif

ISO_SURFACE_FUNC(SampleRoomBody)
{
    TIMED_BLOCK;

    MeshGeneratorRoomData* roomData = (MeshGeneratorRoomData*)samplingData;

    // Box
    v3 d = Abs( worldP.relativeP ) - roomData->dim * 0.5f;
    r32 result = Max( Max( d.x, d.y ), d.z );
    return result;
}

MESH_GENERATOR_FUNC(MeshGeneratorRoomFunc)
{
    Mesh* result = MarchAreaFast( { V3Zero, V3iZero }, V3( generatorData.areaSideMeters ), generatorData.resolutionMeters, SampleRoomBody,
                                  &generatorData.room, samplingCache, meshPool );
    result->mTransform = M4Translation( entityCoords.relativeP );

    return result;
}


ISO_SURFACE_FUNC(RoomSurfaceFunc)
{
    TIMED_BLOCK;

    Room* roomData = (Room*)samplingData;
    // NOTE We're axis aligned for now, so just translate
    v3 invWorldP = worldP.relativeP - roomData->worldP.relativeP;

    // Inflate the size a little
    r32 result = SDFBox( invWorldP, roomData->halfSize + V3( VoxelSizeMeters * 0.5f ) );
    return result;
}



///// MARCHING CUBES LUTs /////

namespace
{
    // Offsets from the minimal corner to other corners
    v3i cornerOffsets[8] =
    {
        V3i( 0, 0, 0 ),    // Bottom layer
        V3i( 1, 0, 0 ),
        V3i( 1, 1, 0 ),
        V3i( 0, 1, 0 ),
        V3i( 0, 0, 1 ),    // Top layer
        V3i( 1, 0, 1 ),
        V3i( 1, 1, 1 ),
        V3i( 0, 1, 1 )
    };

    // Offsets from the minimal corner to 2 ends of the edges
    // (each entry is an index to the previous table)
    u32 edgeVertexOffsets[12][2] =
    {
        { 0, 1 },           // Bottom (horizontal)
        { 1, 2 },
        { 3, 2 },
        { 0, 3 },
        { 4, 5 },           // Top (horizontal)
        { 5, 6 },
        { 7, 6 },
        { 4, 7 },
        { 0, 4 },           // Middle (vertical)
        { 1, 5 },
        { 2, 6 },
        { 3, 7 }
    };

    // Offset to the cell containing the (possibly) cached vertex for each edge
    VertexCacheIndex vertexCacheIndices[12] =
    {
        { { 0, 0 }, 0, 0 },
        { { 1, 0 }, 0, 1 },
        { { 0, 1 }, 0, 0 },
        { { 0, 0 }, 0, 1 },
        { { 0, 0 }, 2, 0 },
        { { 1, 0 }, 2, 1 },
        { { 0, 1 }, 2, 0 },
        { { 0, 0 }, 2, 1 },
        { { 0, 0 }, 1 },
        { { 1, 0 }, 1 },
        { { 1, 1 }, 1 },
        { { 0, 1 }, 1 }
    };

    // List of edge indices for every triangle & for every possible case
    // Up to 15 indices per case, -1 indicates end of sequence
    // TODO Could be packed into a single 64 bit number for speed (4 bits per index, 15+1 indices per case)
    int triangleTable[][16] =
    {
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
        {3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
        {3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
        {3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
        {9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
        {9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
        {2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
        {8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
        {9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
        {4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
        {3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
        {1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
        {4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
        {4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
        {9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
        {5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
        {2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
        {9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
        {0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
        {2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
        {10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
        {4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
        {5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
        {5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
        {9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
        {0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
        {1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
        {10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
        {8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
        {2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
        {7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
        {9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
        {2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
        {11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
        {9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
        {5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
        {11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
        {11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
        {1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
        {9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
        {5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
        {2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
        {5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
        {6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
        {3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
        {6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
        {5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
        {1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
        {10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
        {6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
        {8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
        {7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
        {3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
        {5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
        {0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
        {9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
        {8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
        {5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
        {0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
        {6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
        {10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
        {10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
        {8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
        {1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
        {3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
        {0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
        {10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
        {3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
        {6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
        {9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
        {8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
        {3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
        {6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
        {0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
        {10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
        {10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
        {2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
        {7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
        {7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
        {2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
        {1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
        {11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
        {8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
        {0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
        {7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
        {10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
        {2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
        {6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
        {7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
        {2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
        {1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
        {10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
        {10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
        {0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
        {7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
        {6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
        {8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
        {9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
        {6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
        {4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
        {10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
        {8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
        {0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
        {1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
        {8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
        {10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
        {4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
        {10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
        {5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
        {11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
        {9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
        {6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
        {7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
        {3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
        {7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
        {9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
        {3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
        {6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
        {9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
        {1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
        {4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
        {7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
        {6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
        {3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
        {0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
        {6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
        {0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
        {11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
        {6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
        {5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
        {9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
        {1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
        {1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
        {10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
        {0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
        {5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
        {10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
        {11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
        {9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
        {7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
        {2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
        {8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
        {9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
        {9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
        {1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
        {9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
        {9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
        {5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
        {0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
        {10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
        {2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
        {0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
        {0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
        {9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
        {5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
        {3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
        {5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
        {8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
        {0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
        {9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
        {0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
        {1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
        {3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
        {4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
        {9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
        {11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
        {11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
        {2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
        {9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
        {3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
        {1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
        {4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
        {4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
        {0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
        {3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
        {3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
        {0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
        {9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
        {1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
        {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}
    };
}
