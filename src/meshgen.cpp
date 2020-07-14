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


IsoSurfaceSamplingCache InitSurfaceSamplingCache( MemoryArena* arena, v2i const& cellsPerAxis )
{
    IsoSurfaceSamplingCache result;
    result.cellsPerAxis = cellsPerAxis;

    // NOTE We add an extra row at the ends to account for the edges at the extremes,
    // which simplifies the algorithm by eliminating "edge" cases ¬¬
    v2i stepsPerAxis = cellsPerAxis + V2iOne;
    int layerCellCount = stepsPerAxis.x * stepsPerAxis.y;

    result.bottomLayerSamples = PUSH_ARRAY( arena, f32, layerCellCount );
    result.topLayerSamples = PUSH_ARRAY( arena, f32, layerCellCount );
    result.bottomLayerVertexIndices = PUSH_ARRAY( arena, i32, layerCellCount * 2 );
    result.middleLayerVertexIndices = PUSH_ARRAY( arena, i32, layerCellCount );
    result.topLayerVertexIndices = PUSH_ARRAY( arena, i32, layerCellCount * 2 );

    return result;
}

void ClearVertexCaches( IsoSurfaceSamplingCache* samplingCache, bool clearBottomLayer )
{
    v2i stepsPerAxis = samplingCache->cellsPerAxis + V2iOne;
    int layerCellCount = stepsPerAxis.x * stepsPerAxis.y;

    if( clearBottomLayer )
    {
        PSET( samplingCache->bottomLayerVertexIndices, U32MAX,
             layerCellCount * 2 * sizeof(i32) );
    }
    PSET( samplingCache->middleLayerVertexIndices, U32MAX,
         layerCellCount * sizeof(i32) );
    PSET( samplingCache->topLayerVertexIndices, U32MAX, 
         layerCellCount * 2 * sizeof(i32) );
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
    INIT( &pool->scratchVertices ) BucketArray<TexturedVertex>( arena, 1024 );
    INIT( &pool->scratchIndices ) BucketArray<i32>( arena, 1024 );

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

Mesh* AllocateMesh( MeshPool* pool, int vertexCount, int indexCount )
{
    sz vertexSize = sizeof(TexturedVertex) * vertexCount;
    sz indexSize = sizeof(i32) * indexCount;
    sz totalMeshSize = sizeof(Mesh) + vertexSize + indexSize;

    Mesh* result = nullptr;
    // FIXME This is wrong. Not accounting for the size of the MemoryBlock struct (which we shouldn't have to do anyway!)
    MemoryBlock* block = FindBlockForSize( &pool->memorySentinel, totalMeshSize );
    if( block )
    {
        // TODO Tune this based on the smallest mesh size we will typically have
        const sz blockSplitThreshold = 4096;
        result = (Mesh*)UseBlock( block, totalMeshSize, blockSplitThreshold );

        u8* vertexData = (u8*)result + sizeof(Mesh);
        u8* indexData = vertexData + vertexSize;

        InitMesh( result );
        INIT( &result->vertices ) Array<TexturedVertex>( (TexturedVertex*)vertexData, vertexCount );
        INIT( &result->indices ) Array<i32>( (i32*)indexData, indexCount );
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

    pool->scratchVertices.CopyTo( &result->vertices );
    pool->scratchIndices.CopyTo( &result->indices );

    return result;
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

// TODO Estimate error
// TODO SIMD
v3 QEFMinimizePlanesProbabilistic64( v3 const* points, v3 const* normals, int count, float stdDevP, float stdDevN )
{
	double A00 = 0.0;
	double A01 = 0.0;
	double A02 = 0.0;
	double A11 = 0.0;
	double A12 = 0.0;
	double A22 = 0.0;

	double b0 = 0.0;
	double b1 = 0.0;
	double b2 = 0.0;

	double c = 0.0;

	for( int i = 0; i < count; ++i )
	{
		v3 const& p = points[i];
		v3 const& n = normals[i];

		const double px = p.x;
		const double py = p.y;
		const double pz = p.z;
		const double nx = n.x;
		const double ny = n.y;
		const double nz = n.z;
		const double pn = px * nx + py * ny + pz * nz;

		const double nxny = nx * ny;
		const double nxnz = nx * nz;
		const double nynz = ny * nz;
		const double sn2 = stdDevN * stdDevN;

		const double pA00 = nx * nx + sn2;
		const double pA11 = ny * ny + sn2;
		const double pA22 = nz * nz + sn2;
		A00 += pA00;
		A01 += nxny;
		A02 += nxnz;
		A11 += pA11;
		A12 += nynz;
		A22 += pA22;

		const double pb0 = nx * pn + px * sn2;
		const double pb1 = ny * pn + py * sn2;
		const double pb2 = nz * pn + pz * sn2;
		b0 += pb0;
		b1 += pb1;
		b2 += pb2;

#if 0
		const double sp2 = stdDevP * stdDevP;
		const double pp = px * px + py * py + pz * pz;
		const double nn = nx * nx + ny * ny + nz * nz;
		c += pn * pn + sn2 * pp + sp2 * nn + 3 * sp2 * sn2;
#endif
	}

	// Solving Ax = r with some common subexpressions precomputed
	double A00A12 = A00 * A12;
	double A01A22 = A01 * A22;
	double A11A22 = A11 * A22;
	double A02A12 = A02 * A12;
	double A02A11 = A02 * A11;

	double A01A12_A02A11 = A01 * A12 - A02A11;
	double A01A02_A00A12 = A01 * A02 - A00A12;
	double A02A12_A01A22 = A02A12 - A01A22;

	double denom = A00 * A11A22 + 2.0 * A01 * A02A12 - A00A12 * A12 - A01A22 * A01 - A02A11 * A02;
	ASSERT( denom != 0.0 );
	denom = 1.0 / denom;
	double nom0 = b0 * (A11A22 - A12 * A12) + b1 * A02A12_A01A22 + b2 * A01A12_A02A11;
	double nom1 = b0 * A02A12_A01A22 + b1 * (A00 * A22 - A02 * A02) + b2 * A01A02_A00A12;
	double nom2 = b0 * A01A12_A02A11 + b1 * A01A02_A00A12 + b2 * (A00 * A11 - A01 * A01);

	v3 result = { (float)(nom0 * denom), (float)(nom1 * denom), (float)(nom2 * denom) };
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
    extern i32 edgeVertexOffsets[12][2];
    extern VertexCacheIndex vertexCacheIndices[12];
    extern int triangleTable[][16];
}

void MarchCube( const v3& cellCornerWorldP, const v2i& gridCellP, v2i const& cellsPerAxis, f32 cellSizeMeters,
                IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices, BucketArray<i32>* indices,
                const bool interpolate /*= true*/ )
{
    TIMED_BLOCK;

    // Cache layers contain one sample per _edge_
    v2i layerStepsPerAxis = cellsPerAxis + V2iOne;

    // Construct case mask from 8 corner samples
    int caseIndex = 0;
    f32 cornerSamples[8];
    for( int i = 0; i < 8; ++i )
    {
        v3i cornerOffset = cornerOffsets[i];
        v3i layerP = V3i( gridCellP ) + cornerOffset;
        int layerOffset = layerP.y * layerStepsPerAxis.x + layerP.x;

        f32 sample = cornerOffset.z
            ? samplingCache->topLayerSamples[layerOffset]
            : samplingCache->bottomLayerSamples[layerOffset];

        if( sample >= 0 )
            caseIndex |= 1 << i;

        cornerSamples[i] = sample;
    }

    // Early out if entirely inside or outside
    if( caseIndex == 0 || caseIndex == 0xFF )
        return;

    i32* vertexCaches[3] =
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
            i32* vertexCache = vertexCaches[idx.cacheTableIndex];

            v2i layerP = gridCellP + idx.vCellOffset;
            int vertexCacheOffset = layerP.y * layerStepsPerAxis.x + layerP.x;
            if( idx.cacheTableIndex != 1)
            {
                // Top or bottom layers
                vertexCacheOffset = 2 * vertexCacheOffset + idx.cacheTableOffset;
            }

#if 1       // Use vertex cache (produces around 1/8th vertices)
            i32 cachedVertexIndex = vertexCache[vertexCacheOffset];
            if( cachedVertexIndex != U32MAX )
            {
                indices->Push( cachedVertexIndex );
            }
            else
#endif
            {
                // No cached vertex, so go on and create one
                int indexA = edgeVertexOffsets[edgeCaseIndex][0];   // Edge start
                int indexB = edgeVertexOffsets[edgeCaseIndex][1];   // Edge end

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
                int newVertexIndex = vertices->count;
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

void
MarchVolumeFast( WorldCoords const& worldP, v3 const& volumeSideMeters, f32 cellSizeMeters, IsoSurfaceFunc* sampleFunc,
                 const void* samplingData, IsoSurfaceSamplingCache* samplingCache, BucketArray<TexturedVertex>* vertices,
                 BucketArray<i32>* indices, const bool interpolate = true )
{
    TIMED_BLOCK;

    vertices->Clear();
    indices->Clear();

    // TODO Should do ceil
    v2i cellsPerSliceAxis = V2i( volumeSideMeters.xy / cellSizeMeters );
    int sliceCount = (int)(volumeSideMeters.z / cellSizeMeters);
    ASSERT( samplingCache->cellsPerAxis.x >= cellsPerSliceAxis.x && samplingCache->cellsPerAxis.y >= cellsPerSliceAxis.y );
    v3 halfSideMeters = volumeSideMeters / 2;
    v3 cornerOffset = -halfSideMeters;

    v3 vXDelta = V3( cellSizeMeters, 0.f, 0.f );
    v3 vYDelta = V3( 0.f, cellSizeMeters, 0.f );

    v2i gridLinesPerAxis = cellsPerSliceAxis + V2iOne;
    bool firstSlice = true;

    WorldCoords p = worldP;

    // Iterate slices, we consider both the bottom and the top samples of the cubes on each pass
    for( int k = 0; k < sliceCount; ++k )
    {
        f32* bottomSamples = samplingCache->bottomLayerSamples;
        f32* topSamples = samplingCache->topLayerSamples;

        // Pre-sample top and bottom corners of cubes for each slice (actually only the top for all slices except the first one)
        // so we only sample one corner per cube instead of 8
        for( int n = 0; n < 2; ++n )
        {
            if( n == 0 && !firstSlice )
                continue;

            f32* sampledLayer = n ? topSamples : bottomSamples;

            p.relativeP = worldP.relativeP + cornerOffset + V3i( 0, 0, k + n ) * cellSizeMeters;

            f32* sample = sampledLayer;
            // Iterate grid lines when sampling each layer, since we need to have samples at the extremes too
            for( int j = 0; j < gridLinesPerAxis.y; ++j )
            {
                v3 pAtRowStart = p.relativeP;
                for( int i = 0; i < gridLinesPerAxis.x; ++i )
                {
                    *sample++ = sampleFunc( p, samplingData );
                    p.relativeP += vXDelta;
                }
                p.relativeP = pAtRowStart + vYDelta;
            }
        }

        // Keep a cache of already calculated vertices to eliminate duplication
        ClearVertexCaches( samplingCache, firstSlice );

        p.relativeP = worldP.relativeP + cornerOffset + V3i( 0, 0, k ) * cellSizeMeters;
        for( int j = 0; j < cellsPerSliceAxis.y; ++j )
        {
            v3 pAtRowStart = p.relativeP;
            for( int i = 0; i < cellsPerSliceAxis.x; ++i )
            {
                MarchCube( p.relativeP, V2i( i, j ), cellsPerSliceAxis, cellSizeMeters, samplingCache, vertices, indices, interpolate );
                p.relativeP += vXDelta;
            }
            p.relativeP = pAtRowStart + vYDelta;
        }

        firstSlice = false;
        SwapTopAndBottomLayers( samplingCache );
    }
}



// TODO Clean up asserts
// TODO Clean up asserts
// TODO Clean up asserts
void
DCVolume( WorldCoords const& worldP, v3 const& volumeSideMeters, f32 cellSizeMeters, IsoSurfaceFunc* sampleFunc, const void* samplingData,
          BucketArray<TexturedVertex>* vertices, BucketArray<i32>* indices, MemoryArena* tempArena, DCSettings const& settings )
{
    struct CellData
    {
        // NOTE If we clamped the final SDF field, say -1 to 1, we could probably get away with much smaller fixed-point numbers
        // 0, 1, 2 correspond to the edges aligned to X, Y, Z in each cell
        v3 edgeCrossingsP[3];
        v3 edgeCrossingsN[3];
        // v3 n; // NOTE Unused for now
        // Only one sample per cell (the 'max' corner of the aabb)
        f32 sampledValue;
        i32 vertexIndex;
    };

    // TODO Pack these LUTs so they use less cache
    // ALSO align properly!

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
        i32 cornerA;
        i32 cornerB;
        i32 neighbourIndex;
        i32 storeIndex;
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

    vertices->Clear();
    indices->Clear();

    // One extra layer of cells in X,Y,Z (around the min grid corner) to store the edges and samples at the border
    v3i cellsPerAxis = V3iCeil( volumeSideMeters / cellSizeMeters ) + V3iOne;

    Grid3D<CellData> cellData( tempArena, cellsPerAxis, Temporary() );
    PZERO( cellData.data, cellsPerAxis.x * cellsPerAxis.y * cellsPerAxis.z * sizeof(CellData) );

    v3 halfSideMeters = volumeSideMeters / 2;
    v3 minGridP = worldP.relativeP - halfSideMeters;
    WorldCoords p = worldP;

    const f32 delta = 0.01f;
    const f32 deltaInv = 1.f / (2.f * delta);

    ClusterSamplingData* clusterData = (ClusterSamplingData*)samplingData;
    Cluster* debugCluster = clusterData->debugCluster;
    const v3 debugSampleSize = V3( 0.03f );
    const v4 inColor = { 1, 0, 0, 0 };
    const v4 outColor = { 0, 0, 1, 0 };

    for( int k = 0; k < cellsPerAxis.z; ++k )
    {
        for( int j = 0; j < cellsPerAxis.y; ++j )
        {
            for( int i = 0; i < cellsPerAxis.x; ++i )
            {
                v3 cellP = minGridP + V3( i, j, k ) * cellSizeMeters;

                //f32 boundsTolerance = 0.5f;
                v3 cellBoundsMin = cellP - V3( cellSizeMeters );
                v3 cellBoundsMax = cellP;

                u32 caseMask = 0;
                f32 cornerSamples[8] = {};

                // Build our bitmask using the samples from every corner
                // (each cell only samples its 'max' corner)
                for( int s = 0; s < 8; ++s )
                {
                    cornerSamples[s] = F32MAX;
                    v3i cornerOffset = dcCornerOffsets[s];

                    f32 sample = F32NAN;
                    if( s == 7 )
                    {
                        // Sample our own
                        // Cell at { 0, 0, 0 } (border) gets the sample at world position minGridP + { 0, 0, 0 }
                        // Account for -0 by just adding +0 to the value
                        p.relativeP = cellP;
                        sample = sampleFunc( p, samplingData ) + 0.f;
                        cellData( i, j, k ).sampledValue = sample;

                        // TODO Use instancing and just draw three crossing axis lines at each point to make this viable
#if 0 //!RELEASE
                        if( debugCluster ) //&& (i == 0 || j == 0 || k == 0 || i == cellsPerAxis.x-1 || j == cellsPerAxis.y-1 || k == cellsPerAxis.z-1) )
                        {
                            v4 color = sample >= 0.f ? outColor : inColor;
                            color.a = Clamp01( 1.f - Abs( sample ) / 5.f );
                            debugCluster->debugVolumes.Push( { { cellP, debugSampleSize }, color } );
                        }
#endif

                    }
                    else
                    {
                        v3i cellOffset = V3i( i, j, k ) + dcCornerOffsets[s];
                        // For corners outside the range, sample the SDF anyway so we at least have real values to determine crossings
                        if( cellOffset.x < 0 || cellOffset.y < 0 || cellOffset.z < 0 )
                        {
                            p.relativeP = minGridP + V3( cellOffset ) * cellSizeMeters;
                            sample = sampleFunc( p, samplingData ) + 0.f;
                        }
                        else
                            sample = cellData( cellOffset ).sampledValue;
                    }

                    if( Sign( sample ) )
                        caseMask |= 1 << s;
                    cornerSamples[s] = sample;
                }

                // Early out if entirely inside or outside
                if( caseMask == 0u || caseMask == 0xFFu )
                    continue;

                v2i edges[12];
                v3 edgePoints[12];
                v3 edgeNormals[12];
                int pointCount = 0;

                // We only need to process 3 edges per cell (those containing the corner stored in each cell)
                // Find edge intersections for those, get them from neighbours for the rest
                for( int e = 0; e < 12; ++e )
                {
                    EdgeLocator const& locator = dcEdgeLocators[e];

                    int indexA = locator.cornerA;
                    int indexB = locator.cornerB;
                    f32 sA = cornerSamples[ indexA ];
                    f32 sB = cornerSamples[ indexB ];

                    // Is there a crossing at this edge
                    if( Sign( sA ) != Sign( sB ) )
                    {
                        int neighbourIndex = locator.neighbourIndex;
                        v3i neighbourCoords = V3i( i, j, k ) + dcCornerOffsets[neighbourIndex];
                        bool atOuterEdge = neighbourCoords.z < 0 || neighbourCoords.y < 0 || neighbourCoords.x < 0;

                        if( atOuterEdge || indexB == 7 || indexA == 7 )
                        {
                            // It's one of the edges stored in this cell, so find the intersection & normal
                            v3 pA = cellP + V3( dcCornerOffsets[indexA] ) * cellSizeMeters;
                            v3 pB = cellP + V3( dcCornerOffsets[indexB] ) * cellSizeMeters;
                            v3 edgeP = V3Undefined;

                            static const f32 epsilon = 0.01f;
                            if( sB == F32MAX || AlmostEqual( sA, 0.f, epsilon ) )
                                edgeP = pA;
                            else if( sA == F32MAX || AlmostEqual( sB, 0.f, epsilon ) )
                                edgeP = pB;
                            else
                            {
                                if( settings.approximateEdgeIntersection )
                                {
                                    // Just interpolate along the edge
                                    f32 t = sA / (sA - sB);
                                    Clamp01( t );
                                    edgeP = Lerp( pA, pB, t );
                                }
                                else
                                {
                                    // Do a binary search along the edge
                                    static const int maxSearchSteps = 100;
                                    int searchSteps = maxSearchSteps;
                                    f32 edgeSample = 0.f;
                                    v3 lastP = V3Undefined;

                                    while( --searchSteps )
                                    {
                                        p.relativeP = (pA + pB) * 0.5f;
                                        if( p.relativeP == lastP )
                                        {
                                            // Float precision is limited
                                            edgeP = lastP;
                                            break;
                                        }
                                        lastP = p.relativeP;
                                            
                                        edgeSample = sampleFunc( p, samplingData );

                                        if( AlmostEqual( edgeSample, 0.f, epsilon ) )
                                        {
                                            edgeP = lastP;
                                            break;
                                        }
                                        else
                                        {
                                            if( Sign( edgeSample ) == Sign( sA ) )
                                                pA = lastP;
                                            else
                                                pB = lastP;
                                        }
                                    }
                                    ASSERT( searchSteps > 0 );
                                }
                            }
                            edgePoints[pointCount] = edgeP;
                            if( !atOuterEdge )
                                cellData( i, j, k ).edgeCrossingsP[locator.storeIndex] = edgeP;

                            ASSERT( edgeP != V3Undefined );
                            if( pointCount )
                                ASSERT( Distance( edgePoints[pointCount-1], edgePoints[pointCount] ) < 3.f * VoxelSizeMeters );

                            // Find normal vector by sampling near the intersection point we found
                            p.relativeP = { edgeP.x + delta, edgeP.y, edgeP.z };
                            f32 xPSample = sampleFunc( p, samplingData );
                            p.relativeP = { edgeP.x - delta, edgeP.y, edgeP.z };
                            f32 xNSample = sampleFunc( p, samplingData );

                            p.relativeP = { edgeP.x, edgeP.y + delta, edgeP.z };
                            f32 yPSample = sampleFunc( p, samplingData );
                            p.relativeP = { edgeP.x, edgeP.y - delta, edgeP.z };
                            f32 yNSample = sampleFunc( p, samplingData );

                            p.relativeP = { edgeP.x, edgeP.y, edgeP.z + delta };
                            f32 zPSample = sampleFunc( p, samplingData );
                            p.relativeP = { edgeP.x, edgeP.y, edgeP.z - delta };
                            f32 zNSample = sampleFunc( p, samplingData );

                            v3 normal = V3( xPSample - xNSample, yPSample - yNSample, zPSample - zNSample ) * deltaInv;
                            Normalize( normal );
                            edgeNormals[pointCount] = normal;
                            if( !atOuterEdge )
                                cellData( i, j, k ).edgeCrossingsN[locator.storeIndex] = normal;
                        }
                        else
                        {
                            // This has already been calculated and stored in a neighbour cell so go get it
                            ASSERT( neighbourIndex != 7 );

                            edgePoints[pointCount] = cellData( neighbourCoords ).edgeCrossingsP[locator.storeIndex];
                            edgeNormals[pointCount] = cellData( neighbourCoords ).edgeCrossingsN[locator.storeIndex];

                            if( pointCount )
                                ASSERT( Distance( edgePoints[pointCount-1], edgePoints[pointCount] ) < 3.f * VoxelSizeMeters );
                        }

                        edges[pointCount] = { indexA, indexB };
                        pointCount++;
                    }
                }

                ASSERT( pointCount );

                bool clamped = false;
                v3 cellVertex = V3Undefined;
                switch( settings.cellPointsComputationMethod )
                {
                    case DCComputeMethod::Average:
                    {
                        v3 massPoint = V3Zero;
                        for( int pIndex = 0; pIndex < pointCount; ++pIndex )
                            massPoint += edgePoints[pIndex];
                        massPoint /= (f32)pointCount;

                        cellVertex = massPoint;

                        // TODO When merging cells in the octree, we still need to do a part of the QEF computation, as stated in "The Secret Sauce":
                        // "In addition to the data already stored for the mass point, we also store the dimension of the mass point"
                    } break;
                    case DCComputeMethod::QEFClassic:
                    {
                        cellVertex = QEFMinimizePlanesClassic( edgePoints, edgeNormals, pointCount );
                    } break;
                    case DCComputeMethod::QEFProbabilistic:
                    {
                        cellVertex = QEFMinimizePlanesProbabilistic( edgePoints, edgeNormals, pointCount, 1.f, settings.sigmaN );
                    } break;
                    case DCComputeMethod::QEFProbabilisticDouble:
                    {
                        cellVertex = QEFMinimizePlanesProbabilistic64( edgePoints, edgeNormals, pointCount, 1.f, settings.sigmaNDouble );
                    } break;

                    default:
                        NOT_IMPLEMENTED;
                }

                if( settings.clampCellPoints )
                {
                    // TODO Ideally we should do the solve with constrains as explained in https://www.mattkeeter.com/projects/qef/
                    // (also check https://github.com/BorisTheBrave/mc-dc/blob/a165b326849d8814fb03c963ad33a9faf6cc6dea/qef.py#L146)

                    //Clamp( &cellVertex, cellBounds );
                    if( cellVertex.x < cellBoundsMin.x )
                    {
                        cellVertex.x = cellBoundsMin.x;
                        clamped = true;
                    }
                    if( cellVertex.x > cellBoundsMax.x )
                    {
                        cellVertex.x = cellBoundsMax.x;
                        clamped = true;
                    }
                    if( cellVertex.y < cellBoundsMin.y )
                    {
                        cellVertex.y = cellBoundsMin.y;
                        clamped = true;
                    }
                    if( cellVertex.y > cellBoundsMax.y )
                    {
                        cellVertex.y = cellBoundsMax.y;
                        clamped = true;
                    }
                    if( cellVertex.z < cellBoundsMin.z )
                    {
                        cellVertex.z = cellBoundsMin.z;
                        clamped = true;
                    }
                    if( cellVertex.z > cellBoundsMax.z )
                    {
                        cellVertex.z = cellBoundsMax.z;
                        clamped = true;
                    }
                }
                else
                {
                    //ASSERT( Contains( cellBounds, cellVertex ) );
                }

                int vertexIndex = vertices->count;
                TexturedVertex v = {};
                v.p = cellVertex;
                v.color = clamped ? Pack01ToRGBA( 1, 0, 0, 1 ) : Pack01ToRGBA( 1, 1, 1, 1 );
                vertices->Push( v );

                cellData( i, j, k ).vertexIndex = vertexIndex;
#if 0
                v3 avgNormal = V3Zero;
                for( int p = 0; p < pointCount; ++p )
                    avgNormal += edgeNormals[p];
                avgNormal /= pointCount;
                cellData( i, j, k ).n = avgNormal;
#endif


                // Now we look 'backwards' and create at most 3 quads corresponding to the edges that include the 'min' corner instead,
                // as we know those vertices will be ready by now
                static const i32 edgesProducingPolys[3][2] =
                {
                    { 0, 1 },
                    { 0, 2 },
                    { 0, 4 },
                };

                for( int e = 0; e < 3; ++e )
                {
                    f32 sA = cornerSamples[ edgesProducingPolys[e][0] ];
                    f32 sB = cornerSamples[ edgesProducingPolys[e][1] ];

                    if( Sign( sA ) != Sign( sB ) )
                    {
                        switch( e )
                        {
                        case 0:     // Normal aligned to +/- X
                        {
                            if( k == 0 || j == 0 )
                                break;

                            if( sA < sB )
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );

                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i, j-1, k-1 ).vertexIndex ); 
                            }
                            else
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );

                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i, j-1, k-1 ).vertexIndex ); 
                            }
                        } break;
                        case 1:     // Normal aligned to +/- Y
                        {
                            if( k == 0 || i == 0 )
                                break;

                            if( sA < sB )
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );

                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i-1, j, k-1 ).vertexIndex );
                            }
                            else
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i, j, k-1 ).vertexIndex );

                                indices->Push( cellData( i, j, k-1 ).vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i-1, j, k-1 ).vertexIndex );
                            }
                        } break;
                        case 2:     // Normal aligned to +/- Z
                        {
                            if( j == 0 || i == 0 )
                                break;

                            if( sA < sB )
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );

                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i-1, j-1, k).vertexIndex );
                            }
                            else
                            {
                                indices->Push( vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i-1, j, k ).vertexIndex );

                                indices->Push( cellData( i-1, j, k ).vertexIndex );
                                indices->Push( cellData( i, j-1, k ).vertexIndex );
                                indices->Push( cellData( i-1, j-1, k).vertexIndex );
                            }
                        } break;
                        }
                    }
                }
            }
        }
    }
}

#define VALUES(x) \
    x(DualContouring) \
    x(MarchingCubes) \

STRUCT_ENUM(ContouringTechnique, VALUES)
#undef VALUES



struct Metaball
{
    v3 pCenter;
    f32 radiusMeters;
};

ISO_SURFACE_FUNC(SampleMetaballs)
{
    const Array<Metaball>& balls = *(const Array<Metaball>*)samplingData;

    f32 minValue = F32MAX;
    for( int i = 0; i < balls.capacity; ++i )
    {
        f32 value = DistanceSq( balls[i].pCenter, worldP.relativeP ) - balls[i].radiusMeters;
        if( value < minValue )
            minValue = value;
    }

    return minValue;
}

#if 0
void
TestMetaballs( float areaSideMeters, float cellSizeMeters, float elapsedT, IsoSurfaceSamplingCache* samplingCache,
               MeshPool* meshPool, RenderCommands *renderCommands )
{
    persistent ARRAY(Metaball, 10, balls);

    if( balls[0].radiusMeters == 0 )
    {
        for( int i = 0; i < balls.count; ++i )
        {
            //f32 x = RandomRange( -halfSideMeters, halfSideMeters );
            //f32 y = RandomRange( -halfSideMeters, halfSideMeters );
            //f32 z = RandomRange( -halfSideMeters, halfSideMeters );
            f32 r = RandomRangeF32( 1.0f, 5.0f );
            balls.Push( { V3Zero, r } );
        }
    }

    // Update positions
    for( int i = 0; i < balls.count; ++i )
    {
        Metaball& ball = balls[i];
        f32 x = (i+1) / 2.0f * Cos( elapsedT - i );
        f32 y = (i+2) / 2.0f * Sin( elapsedT + i );
        f32 z = (i+3) / 2.0f * Sin( elapsedT - i );

        ball.pCenter = { x, y, z };
    }
    
    // Update mesh by sampling our cubic area centered at origin
    Mesh* metaMesh = MarchVolumeFast( { V3Zero, V3iZero }, V3( areaSideMeters ), cellSizeMeters,
                                      SampleMetaballs, &balls,
                                      samplingCache, meshPool );

    RenderMesh( *metaMesh, renderCommands );
}
#endif

ISO_SURFACE_FUNC( TorusSurfaceFunc )
{
    // NOTE We're axis aligned for now, so just translate
    v3 invWorldP = worldP.relativeP - V3Zero;

    f32 result = SDFTorus( invWorldP, 70, 30 );
    return result;
}

ISO_SURFACE_FUNC( BoxSurfaceFunc )
{
    // NOTE We're axis aligned for now, so just translate
    v3 invWorldP = worldP.relativeP - V3Zero;

    f32 result = SDFBox( invWorldP, { 70, 70, 70 } );
    return result;
}

#define SURFACE_LIST(x) \
    x(MechanicalPart)   \
    x(Torus)            \
    x(HollowCube)       \
    x(Devil)            \
    x(QuarticCylinder)  \
    x(TangleCube)       \
    x(TrefoilKnot)      \
    x(Genus2)           \
    //x(Cone)            \
    //x(LinkedTorii)      \

STRUCT_ENUM(SimpleSurface, SURFACE_LIST);
#undef SURFACE_LIST


struct SamplingData
{
    m4 invWorldTransform;
    i32 surfaceType;
};


ISO_SURFACE_FUNC( SimpleSurfaceFunc )
{
    SamplingData* data = (SamplingData*)samplingData;
    int surfaceIndex = data->surfaceType;

    // NOTE Don't care about translation
    v3 const& invWorldP = Transform( data->invWorldTransform, worldP.relativeP );

    f32 result = F32INF;
    switch( surfaceIndex )
    {
        case SimpleSurface::Torus().index:
            result = SDFTorus( invWorldP, 70, 30 );
            break;
        //case SimpleSurface::Cone().index:
            //result = SDFCone( invWorldP, 0.5, 100 );
            //break;
        //case SimpleSurface::LinkedTorii().index:
            //result = SDFLinkedTorii( invWorldP, 45, 20, 25 );
            //break;
        case SimpleSurface::HollowCube().index:
            result = SDFHollowCube( invWorldP );
            break;
        case SimpleSurface::Devil().index:
            result = SDFDevil( invWorldP );
            break;
        case SimpleSurface::QuarticCylinder().index:
            result = SDFQuarticCylinder( invWorldP );
            break;
        case SimpleSurface::TangleCube().index:
            result = SDFTangleCube( invWorldP );
            break;
        case SimpleSurface::TrefoilKnot().index:
            result = SDFTrefoilKnot( invWorldP );
            break;
        case SimpleSurface::Genus2().index:
            result = SDFGenus2( invWorldP );
            break;

        case SimpleSurface::MechanicalPart().index:
        {
            f32 b = SDFBox( invWorldP, { 50, 50, 50 } );
            f32 c = SDFCylinder( invWorldP, 40 );
            result = SDFUnion( b, c );

            v3 yRotP = { invWorldP.z, invWorldP.y, invWorldP.x };
            f32 c1 = SDFCylinder( yRotP, 30 );
            result = SDFSubstraction( result, c1 );
            v3 zRotP = { -invWorldP.y, invWorldP.x, invWorldP.z };
            f32 c2 = SDFCylinder( zRotP, 30 );
            result = SDFSubstraction( result, c2 );
        } break;
    }
    return result;
}






///// MESH SIMPLIFICATION /////

internal inline f64
FQSVertexError( const m4Symmetric& q, f64 x, f64 y, f64 z )
{
    // Error between vertex and quadric
    f64 result = q.e[0]*x*x + 2*q.e[1]*x*y + 2*q.e[2]*x*z + 2*q.e[3]*x
               + q.e[4]*y*y + 2*q.e[5]*y*z + 2*q.e[6]*y
               + q.e[7]*z*z + 2*q.e[8]*z
               + q.e[9];
    return result;
}

internal f64
FQSCalculateError( FQSMesh* mesh, int v1Idx, int v2Idx, v3* result )
{
    FQSVertex& v1 = mesh->vertices[v1Idx];
    FQSVertex& v2 = mesh->vertices[v2Idx];

    // Compute interpolated vertex
    m4Symmetric q = v1.q + v2.q;
    bool border = v1.border && v2.border;
    f64 error = 0;
    f64 det = Determinant3x3( q, 0, 1, 2, 1, 4, 5, 2, 5, 7 );

    // TODO Epsilon?
    if( det != 0 && !border )
    {
        // Invertible
        result->x = (f32)(-1 / det * Determinant3x3( q, 1, 2, 3, 4, 5, 6, 5, 7, 8 ));
        result->y = (f32)( 1 / det * Determinant3x3( q, 0, 2, 3, 1, 5, 6, 2, 7, 8 ));
        result->z = (f32)(-1 / det * Determinant3x3( q, 0, 1, 3, 1, 4, 6, 2, 5, 8 ));
        error = FQSVertexError( q, result->x, result->y, result->z );
    }
    else
    {
        // Try to find best result
        v3 p1 = v1.p;
        v3 p2 = v2.p;
        v3 p3 = (p1 + p2) / 2;
        f64 error1 = FQSVertexError( q, p1.x, p1.y, p1.z );
        f64 error2 = FQSVertexError( q, p2.x, p2.y, p2.z );
        f64 error3 = FQSVertexError( q, p3.x, p3.y, p3.z );
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

internal bool
FQSFlipped( const FQSMesh& mesh, const Array<FQSVertexRef>& refs,
            const v3& p, int i0, int i1, const FQSVertex& v0, const FQSVertex& v1, Array<bool>* deleted )
{
    for( int k = 0; k < v0.refCount; ++k )
    {
        const FQSTriangle& tri = mesh.triangles[refs[v0.refStart + k].tId];
        if( tri.deleted )
            continue;

        int s = refs[v0.refStart + k].tVertex;
        int id1 = tri.v[(s+1)%3];
        int id2 = tri.v[(s+2)%3];

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
FQSUpdateTriangles( int i0, const FQSVertex& v, const Array<bool>& deleted,
                    FQSMesh* mesh, Array<FQSVertexRef>* refs, i32* deleteTriangleCount )
{
    v3 p;
    for( int k = 0; k < v.refCount; ++k )
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
        tri.error[0] = FQSCalculateError( mesh, tri.v[0], tri.v[1], &p );
        tri.error[1] = FQSCalculateError( mesh, tri.v[1], tri.v[2], &p );
        tri.error[2] = FQSCalculateError( mesh, tri.v[2], tri.v[0], &p );
        tri.error[3] = Min( tri.error[0], Min( tri.error[1], tri.error[2] ) );
        refs->Push( ref );
    }
}

// Taken from https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
// Simplification by edge contraction based on quadric error metrics
// TODO See if we can record the decimation process similar to what BunnyLOD does (commented out below) and animate the process in reverse
// to achieve a "magically forming" effect for structures that couldn't be contoured in time due to fast camera movements.
// This would also have the advantage of being able to generate all LODs in a single pass.
// Also, see if we can find an ultra-fast method mostly for coplanar regions and have it applied always after contouring regardless of LOD
// (or make one!)
void FastQuadricSimplify( FQSMesh* mesh, int targetTriCount, const TemporaryMemory& tmpMemory,
                          f32 agressiveness = 7 )
{
    int triangleCount = mesh->triangles.count;
    int deletedTriangleCount = 0;

    for( int i = 0; i < triangleCount; ++i )
        mesh->triangles[i].deleted = false;

    // Need extra space for FQSUpdateTriangles below
    Array<FQSVertexRef> refs( tmpMemory.arena, triangleCount * 3 * 3, Temporary() );
    for( int iteration = 0; iteration < 100; ++iteration )
    {
        if( triangleCount - deletedTriangleCount <= targetTriCount )
            break;

        // Update mesh every few cycles (including first time through)
        if( iteration % 5 == 0 )
        {
            // Init quadrics plane & edge errors
            // Required at the first iteration. Not required later, but could improve the result for closed meshes
            if( iteration == 0 )
            {
                for( int i = 0; i < mesh->vertices.count; ++i )
                    mesh->vertices[i].q = M4SymmetricZero;

                for( int i = 0; i < mesh->triangles.count; ++i )
                {
                    FQSTriangle& tri = mesh->triangles[i];
                    v3 p[3] =
                    {
                        mesh->vertices[tri.v[0]].p,
                        mesh->vertices[tri.v[1]].p,
                        mesh->vertices[tri.v[2]].p
                    };

                    v3 n = Normalized( Cross( p[1] - p[0], p[2] - p[0] ) );
                    tri.n = n;

                    for( int j = 0; j < 3; ++j )
                    {
                        mesh->vertices[tri.v[j]].q += M4Symmetric( n.x, n.y, n.z, -Dot( n, p[0] ) );
                    }
                }

                for( int i = 0; i < mesh->triangles.count; ++i )
                {
                    FQSTriangle& tri = mesh->triangles[i];
                    v3 p;

                    for( int j = 0; j < 3; ++j )
                        tri.error[j] = FQSCalculateError( mesh, tri.v[j], tri.v[(j+1)%3], &p );

                    tri.error[3] = Min( tri.error[0], Min( tri.error[1], tri.error[2] ) );
                }
            }
            else
            {
                // Compact tri array
                int dst = 0;
                for( int i = 0; i < mesh->triangles.count; ++i )
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

            // Rebuild refs list
            for( int i = 0; i < mesh->vertices.count; ++i )
            {
                mesh->vertices[i].refStart = 0;
                mesh->vertices[i].refCount = 0;
            }
            for( int i = 0; i < mesh->triangles.count; ++i )
            {
                FQSTriangle& tri = mesh->triangles[i];
                for( int j = 0; j < 3; ++j )
                    mesh->vertices[tri.v[j]].refCount++;
            }

            int refStart = 0;
            for( int i = 0; i < mesh->vertices.count; ++i )
            {
                FQSVertex& v = mesh->vertices[i];
                v.refStart = refStart;
                refStart += v.refCount;
                v.refCount = 0;
            }

            refs.Resize( mesh->triangles.count * 3 );
            for( int i = 0; i < mesh->triangles.count; ++i )
            {
                FQSTriangle& tri = mesh->triangles[i];
                for( int j = 0; j < 3; ++j )
                {
                    FQSVertex& v = mesh->vertices[tri.v[j]];
                    FQSVertexRef& r = refs[v.refStart + v.refCount];

                    r.tId = i;
                    r.tVertex = j;
                    v.refCount++;
                }
            }

            // Identify boundary vertices
            if( iteration == 0 )
            {
                Array<i32> vCount( tmpMemory.arena, 1000, Temporary() );
                Array<i32> vIds( tmpMemory.arena, 1000, Temporary() );

                for( int i = 0; i < mesh->vertices.count; ++i )
                    mesh->vertices[i].border = false;
                
                for( int i = 0; i < mesh->vertices.count; ++i )
                {
                    vCount.count = 0;
                    vIds.count = 0;

                    FQSVertex& v = mesh->vertices[i];

                    for( int j = 0; j < v.refCount; ++j )
                    {
                        int tId = refs[v.refStart + j].tId;
                        FQSTriangle& tri = mesh->triangles[tId];

                        for( int k = 0; k < 3; ++k )
                        {
                            int ofs = 0;
                            int id = tri.v[k];
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

                    for( int j = 0; j < vCount.count; ++j )
                    {
                        if( vCount[j] == 1 )
                            mesh->vertices[vIds[j]].border = true;
                    }
                }
            }
        }

        for( int i = 0; i < mesh->triangles.count; ++i )
            mesh->triangles[i].dirty = false;

        // All triangles with edges below the threshold will be removed
        // NOTE The following numbers usually work. Adjust as needed
        f64 threshold = 0.000000001 * PowF64( f64(iteration + 3), agressiveness );

        if( iteration % 5 == 0 )
            LOG( "Iteration %d - tris %d  threshold %g", iteration, triangleCount - deletedTriangleCount, threshold );

        for( int i = 0; i < mesh->triangles.count; ++i )
        {
            FQSTriangle& tri = mesh->triangles[i];
            if( tri.error[3] > threshold || tri.deleted || tri.dirty )
                continue;

            for( int j = 0; j < 3; ++j )
            {
                if( tri.error[j] < threshold )
                {
                    Array<bool> deleted0( tmpMemory.arena, 1000, Temporary() );
                    Array<bool> deleted1( tmpMemory.arena, 1000, Temporary() );

                    int i0 = tri.v[j];          FQSVertex& v0 = mesh->vertices[i0];
                    int i1 = tri.v[(j+1)%3];    FQSVertex& v1 = mesh->vertices[i1];

                    if( v0.border != v1.border )
                        continue;

                    // Compute vertex to collapse to
                    v3 p;
                    FQSCalculateError( mesh, i0, i1, &p );

                    deleted0.count = v0.refCount;
					deleted1.count = v1.refCount;

					// Don't remove if flipped
					if( FQSFlipped( *mesh, refs, p, i0, i1, v0, v1, &deleted0 ) )
					    continue;
					if( FQSFlipped( *mesh, refs, p, i1, i0, v1, v0, &deleted1 ) )
					    continue;

                    v0.p = p;
                    v0.q = v1.q + v0.q;

                    int refStart = refs.count;
                    FQSUpdateTriangles( i0, v0, deleted0, mesh, &refs, &deletedTriangleCount );
                    FQSUpdateTriangles( i0, v1, deleted1, mesh, &refs, &deletedTriangleCount );
                    int refCount = refs.count - refStart;

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

    // Compact mesh
    {
        for( int i = 0; i < mesh->vertices.count; ++i )
            mesh->vertices[i].refCount = 0;

        int dst = 0;
        for( int i = 0; i < mesh->triangles.count; ++i )
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

        for( int i = 0; i < mesh->vertices.count; ++i )
        {
            FQSVertex& v = mesh->vertices[i];
            if( v.refCount )
            {
                v.refStart = dst;
                mesh->vertices[dst].p = v.p;
                dst++;
            }
        }

        for( int i = 0; i < mesh->triangles.count; ++i )
        {
            FQSTriangle& tri = mesh->triangles[i];
            for( int j = 0; j < 3; ++j )
                tri.v[j] = mesh->vertices[tri.v[j]].refStart;
        }
        mesh->vertices.count = dst;
    }
}


FQSMesh CreateFQSMesh( Array<TexturedVertex> const& vertices, Array<i32> const& indices, TemporaryMemory const& tmpMemory )
{
    FQSMesh result;
    INIT( &result.vertices ) Array<FQSVertex>( tmpMemory.arena, vertices.count, Temporary() );
    result.vertices.ResizeToCapacity();

    for( int i = 0; i < result.vertices.count; ++i )
        result.vertices[i].p = vertices[i].p;

    INIT( &result.triangles ) Array<FQSTriangle>( tmpMemory.arena, indices.count / 3, Temporary() );
    result.triangles.Resize( result.triangles.capacity );
    for( int i = 0; i < result.triangles.count; ++i )
    {
        result.triangles[i].v[0] = indices[ i*3 + 0 ];
        result.triangles[i].v[1] = indices[ i*3 + 1 ];
        result.triangles[i].v[2] = indices[ i*3 + 2 ];
    }

    return result;
}



#if 0
// BunnyLODSimplify
// NOTE Could be made reasonable by keeping a separate list of costs that is kept sorted always, hence choosing the minimum
// cost edge would be a constant time operation

// Even simpler, we could just have an increasing threshold mechanism like FQS above, and just remove all edges above that
// threshold on each pass

#include <vector>

class Triangle;
class Vertex;

class Triangle {
  public:
	Vertex *         vertex[3]; // the 3 points that make this tri
	v3           normal;    // unit vector othogonal to this face
	                 Triangle(Vertex *v0,Vertex *v1,Vertex *v2);
	                 ~Triangle();
	void             ComputeNormal();
	void             ReplaceVertex(Vertex *vold,Vertex *vnew);
	int              HasVertex(Vertex *v);
};
class Vertex {
  public:
	v3           position; // location of point in euclidean space
	int              id;       // place of vertex in original Array
	std::vector<Vertex *>   neighbor; // adjacent vertices
	std::vector<Triangle *> face;     // adjacent triangles
	float            objdist;  // cached cost of collapsing edge
	Vertex *         collapse; // candidate vertex for collapse
	                 Vertex(v3 v,int _id);
	                 ~Vertex();
	void             RemoveIfNonNeighbor(Vertex *n);
};

struct tridata {
	int	v[3];  // indices to vertex Array
	// texture and vertex normal info removed for this demo
};

std::vector<Vertex *>   vertices;
std::vector<Triangle *> triangles;


template<class T> int   Contains(const std::vector<T> & c, const T & t){ return (int)std::count(begin(c), end(c), t); }
template<class T> int   IndexOf(const std::vector<T> & c, const T & v) { return (int)( std::find(begin(c), end(c), v) - begin(c) ); } // Note: Not presently called
template<class T> T &   Add(std::vector<T> & c, T t)                   { c.push_back(t); return c.back(); }
template<class T> T     Pop(std::vector<T> & c)                        { auto val = std::move(c.back()); c.pop_back(); return val; }
template<class T> void  AddUnique(std::vector<T> & c, T t)             { if (!Contains(c, t)) c.push_back(t); }
template<class T> void  Remove(std::vector<T> & c, T t)                { auto it = std::find(begin(c), end(c), t); ASSERT(it != end(c)); c.erase(it); ASSERT(!Contains(c, t)); }

Triangle::Triangle(Vertex *v0,Vertex *v1,Vertex *v2){
	ASSERT(v0!=v1 && v1!=v2 && v2!=v0);
	vertex[0]=v0;
	vertex[1]=v1;
	vertex[2]=v2;
	ComputeNormal();
	triangles.push_back(this);
	for(int i=0;i<3;i++) {
		vertex[i]->face.push_back(this);
		for(int j=0;j<3;j++) if(i!=j) {
			AddUnique(vertex[i]->neighbor, vertex[j]);
		}
	}
}
Triangle::~Triangle(){
	Remove(triangles,this);
	for(int i=0;i<3;i++) {
		if(vertex[i]) Remove(vertex[i]->face,this);
	}
	for (int i = 0; i<3; i++) {
		int i2 = (i+1)%3;
		if(!vertex[i] || !vertex[i2]) continue;
		vertex[i ]->RemoveIfNonNeighbor(vertex[i2]);
		vertex[i2]->RemoveIfNonNeighbor(vertex[i ]);
	}
}
int Triangle::HasVertex(Vertex *v) {
	return (v==vertex[0] ||v==vertex[1] || v==vertex[2]);
}
void Triangle::ComputeNormal()
{
	v3 v0=vertex[0]->position;
	v3 v1=vertex[1]->position;
	v3 v2=vertex[2]->position;
	normal = Cross(v1-v0,v2-v1);
	if(Length(normal)==0)return;
	normal = Normalized(normal);
}

void Triangle::ReplaceVertex(Vertex *vold,Vertex *vnew) 
{
	ASSERT(vold && vnew);
	ASSERT(vold==vertex[0] || vold==vertex[1] || vold==vertex[2]);
	ASSERT(vnew!=vertex[0] && vnew!=vertex[1] && vnew!=vertex[2]);
	if(vold==vertex[0]){
		vertex[0]=vnew;
	}
	else if(vold==vertex[1]){
		vertex[1]=vnew;
	}
	else {
		ASSERT(vold==vertex[2]);
		vertex[2]=vnew;
	}
	Remove(vold->face,this);
	ASSERT(!Contains(vnew->face,this));
	vnew->face.push_back(this);
	for (int i = 0; i<3; i++) {
		vold->RemoveIfNonNeighbor(vertex[i]);
		vertex[i]->RemoveIfNonNeighbor(vold);
	}
	for (int i = 0; i<3; i++) {
		ASSERT(Contains(vertex[i]->face,this)==1);
		for(int j=0;j<3;j++) if(i!=j) {
			AddUnique(vertex[i]->neighbor,vertex[j]);
		}
	}
	ComputeNormal();
}

Vertex::Vertex(v3 v,int _id) {
	position =v;
	id=_id;
	vertices.push_back(this);
}

Vertex::~Vertex(){
	ASSERT(face.size() == 0);
	while(neighbor.size()) {
		Remove(neighbor[0]->neighbor,this);
		Remove(neighbor,neighbor[0]);
	}
	Remove(vertices,this);
}
void Vertex::RemoveIfNonNeighbor(Vertex *n) {
	// removes n from neighbor Array if n isn't a neighbor.
	if(!Contains(neighbor,n)) return;
	for (unsigned int i = 0; i<face.size(); i++) {
		if(face[i]->HasVertex(n)) return;
	}
	Remove(neighbor,n);
}


float ComputeEdgeCollapseCost(Vertex *u,Vertex *v) {
	// if we collapse edge uv by moving u to v then how 
	// much different will the model change, i.e. how much "error".
	// Texture, vertex normal, and border vertex code was removed
	// to keep this demo as simple as possible.
	// The method of determining cost was designed in order 
	// to exploit small and coplanar regions for
	// effective polygon reduction.
	// Is is possible to add some checks here to see if "folds"
	// would be generated.  i.e. normal of a remaining face gets
	// flipped.  I never seemed to run into this problem and
	// therefore never added code to detect this case.
	float edgelength = Length(v->position - u->position);
	float curvature=0;

	// find the "sides" triangles that are on the edge uv
	std::vector<Triangle *> sides;
	for (unsigned int i = 0; i<u->face.size(); i++) {
		if(u->face[i]->HasVertex(v)){
			sides.push_back(u->face[i]);
		}
	}
	// use the triangle facing most away from the sides 
	// to determine our curvature term
	for (unsigned int i = 0; i<u->face.size(); i++) {
		float mincurv=1; // curve for face i and closer side to it
		for (unsigned int j = 0; j<sides.size(); j++) {
			float dotprod = Dot(u->face[i]->normal , sides[j]->normal);	  // use dot product of face normals. 
			mincurv = Min(mincurv,(1-dotprod)/2.0f);
		}
		curvature = Max(curvature, mincurv);
	}
	// the more coplanar the lower the curvature term   
	return edgelength * curvature;
}

void ComputeEdgeCostAtVertex(Vertex *v) {
	// compute the edge collapse cost for all edges that start
	// from vertex v.  Since we are only interested in reducing
	// the object by selecting the min cost edge at each step, we
	// only cache the cost of the least cost edge at this vertex
	// (in member variable collapse) as well as the value of the 
	// cost (in member variable objdist).
	if (v->neighbor.size() == 0) {
		// v doesn't have neighbors so it costs nothing to collapse
		v->collapse=NULL;
		v->objdist=-0.01f;
		return;
	}
	v->objdist = 1000000;
	v->collapse=NULL;
	// search all neighboring edges for "least cost" edge
	for (unsigned int i = 0; i<v->neighbor.size(); i++) {
		float dist;
		dist = ComputeEdgeCollapseCost(v,v->neighbor[i]);
		if(dist<v->objdist) {
			v->collapse=v->neighbor[i];  // candidate for edge collapse
			v->objdist=dist;             // cost of the collapse
		}
	}
}
void ComputeAllEdgeCollapseCosts() {
	// For all the edges, compute the difference it would make
	// to the model if it was collapsed.  The least of these
	// per vertex is cached in each vertex object.
	for (unsigned int i = 0; i<vertices.size(); i++) {
		ComputeEdgeCostAtVertex(vertices[i]);
	}
}

void Collapse(Vertex *u,Vertex *v){
	// Collapse the edge uv by moving vertex u onto v
	// Actually remove tris on uv, then update tris that
	// have u to have v, and then remove u.
	if(!v) {
		// u is a vertex all by itself so just delete it
		delete u;
		return;
	}
	std::vector<Vertex *>tmp;
	// make tmp a Array of all the neighbors of u
	for (unsigned int i = 0; i<u->neighbor.size(); i++) {
		tmp.push_back(u->neighbor[i]);
	}
	// delete triangles on edge uv:
	{
		auto i = u->face.size();
		while (i--) {
			if (u->face[i]->HasVertex(v)) {
				delete(u->face[i]);
			}
		}
	}
	// update remaining triangles to have v instead of u
	{
		auto i = u->face.size();
		while (i--) {
			u->face[i]->ReplaceVertex(u, v);
		}
	}
	delete u; 
	// recompute the edge collapse costs for neighboring vertices
	for (unsigned int i = 0; i<tmp.size(); i++) {
		ComputeEdgeCostAtVertex(tmp[i]);
	}
}

void AddVertex(std::vector<v3> &vert){
	for (unsigned int i = 0; i<vert.size(); i++) {
		Vertex *v = new Vertex(vert[i],i);
	}
}
void AddFaces(std::vector<tridata> &tri){
	for (unsigned int i = 0; i<tri.size(); i++) {
		Triangle *t=new Triangle(
					      vertices[tri[i].v[0]],
					      vertices[tri[i].v[1]],
					      vertices[tri[i].v[2]] );
	}
}

Vertex *MinimumCostEdge(){
	// Find the edge that when collapsed will affect model the least.
	// This funtion actually returns a Vertex, the second vertex
	// of the edge (collapse candidate) is stored in the vertex data.
	// Serious optimization opportunity here: this function currently
	// does a sequential search through an unsorted Array :-(
	// Our algorithm could be O(n*lg(n)) instead of O(n*n)
	Vertex *mn=vertices[0];
	for (unsigned int i = 0; i<vertices.size(); i++) {
		if(vertices[i]->objdist < mn->objdist) {
			mn = vertices[i];
		}
	}
	return mn;
}

// TODO Use this feature as a way to make entities/structure appear "magically" in the game,
// even use it for structures that weren't contoured in time to be shown properly due to too fast camera movement
/*
 *  The function ProgressiveMesh() takes a model in an "indexed face 
 *  set" sort of way.  i.e. Array of vertices and Array of triangles.
 *  The function then does the polygon reduction algorithm
 *  internally and reduces the model all the way down to 0
 *  vertices and then returns the order in which the
 *  vertices are collapsed and to which neighbor each vertex
 *  is collapsed to.  More specifically the returned "permutation"
 *  indicates how to reorder your vertices so you can render
 *  an object by using the first n vertices (for the n 
 *  vertex version).  After permuting your vertices, the
 *  map Array indicates to which vertex each vertex is collapsed to.
 */
void ProgressiveMesh(std::vector<v3> &vert, std::vector<tridata> &tri,
                     std::vector<int> &map, std::vector<int> &permutation)
{
	AddVertex(vert);  // put input data into our data structures
	AddFaces(tri);
	ComputeAllEdgeCollapseCosts(); // cache all edge collapse costs
	permutation.resize(vertices.size());  // allocate space
	map.resize(vertices.size());          // allocate space
	// reduce the object down to nothing:
	size_t sizeofVerts = vertices.size();
	while( sizeofVerts > 0 ) {
		// get the next vertex to collapse
		Vertex *mn = MinimumCostEdge();
		// keep track of this vertex, i.e. the collapse ordering
		permutation[mn->id] = (int)vertices.size() - 1;
		// keep track of vertex to which we collapse to
		map[vertices.size() - 1] = (mn->collapse) ? mn->collapse->id : -1;
		// Collapse this edge
		Collapse(mn,mn->collapse);

        sizeofVerts = vertices.size();

        printf( "%llu\n", sizeofVerts );
	}
	// reorder the map Array based on the collapse ordering
	for (unsigned int i = 0; i<map.size(); i++) {
		map[i] = (map[i]==-1)?0:permutation[map[i]];
	}
	// The caller of this function should reorder their vertices
	// according to the returned "permutation".
}

int Map(int a,int mx, std::vector<int> const& collapse_map) {
	if(mx<=0) return 0;
	while(a>=mx) {  
		a=collapse_map[a];
	}
	return a;
}


// Adapted from https://github.com/dougbinks/BunnyLOD. Based on the article http://www.melax.com/gdmag.pdf
void BunnyLODSimplify( Array<TexturedVertex>& srcVertices, Array<i32>& srcIndices )
{
    std::vector<v3> vert;       // global Array of vertices
    std::vector<tridata> tri;       // global Array of triangles
    std::vector<int> collapse_map;  // to which neighbor each vertex collapses
	std::vector<int> permutation;

    {
        // Copy the geometry from the arrays of data in rabdata.cpp into
        // the vert and tri Arrays which we send to the reduction routine
        for( int i=0;i<srcVertices.count;i++) {
            vert.push_back( srcVertices[i].p );
        }
        for( int i=0;i<srcIndices.count;i+=3) {
            tridata td;
            td.v[0]=srcIndices[i+0];
            td.v[1]=srcIndices[i+1];
            td.v[2]=srcIndices[i+2];
            tri.push_back(td);
        }
    }
    int render_num=(int)vert.size();  // by default lets use all the model to render

    ProgressiveMesh( vert, tri, collapse_map, permutation );

    {
        // rearrange the vertex Array 
        std::vector<v3> temp_Array;
        unsigned int i;
        ASSERT(permutation.size() == vert.size());
        for (i = 0; i<vert.size(); i++) {
            temp_Array.push_back(vert[i]);
        }
        for(i=0;i<vert.size();i++) {
            vert[permutation[i]]=temp_Array[i];
        }
        // update the changes in the entries in the triangle Array
        for (i = 0; i<tri.size(); i++) {
            for(int j=0;j<3;j++) {
                tri[i].v[j] = permutation[tri[i].v[j]];
            }
        }
    }

    for( int i = 0; i < vert.size(); ++i )
        // TODO Take care of the rest of the attributes
        srcVertices[i].p = vert[i];

    render_num = 10000;
    srcIndices.Resize( render_num*3 );

	for (unsigned int i = 0; i<tri.size(); i++) {
		int p0= Map(tri[i].v[0],render_num, collapse_map);
		int p1= Map(tri[i].v[1],render_num, collapse_map);
		int p2= Map(tri[i].v[2],render_num, collapse_map);
		// note:  serious optimization opportunity here,
		//  by sorting the triangles the following "continue" 
		//  could have been made into a "break" statement.
		if(p0==p1 || p1==p2 || p2==p0) continue;

		v3 v0, v1, v2;
#if 0
		// if we are not currenly morphing between 2 levels of detail
		// (i.e. if morph=1.0) then q0,q1, and q2 are not necessary.
		int q0= Map(p0,(int)(render_num*lodbase));
		int q1= Map(p1,(int)(render_num*lodbase));
		int q2= Map(p2,(int)(render_num*lodbase));
		v0 = vert[p0]*morph + vert[q0]*(1-morph);
		v1 = vert[p1]*morph + vert[q1]*(1-morph);
		v2 = vert[p2]*morph + vert[q2]*(1-morph);
#endif

        srcIndices[i+0] = p0;
        srcIndices[i+1] = p1;
        srcIndices[i+2] = p2;
	}
}

#endif



///// CONVERSION TO 'SAMPLED' MESHES /////

struct Hit
{
    v2i gridCoords;
    f32 hitCoord;
};

internal void
AddSorted( const Hit& hit, Array<Hit>* result )
{
    Array<Hit>& out = *result;
    out.PushEmpty();

    int i = out.count - 1;
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
    for( int i = 0; i < hits.count; ++i )
    {
        if( hits[i].gridCoords == gridCoords )
            AddSorted( hits[i], result );
    }
}

Mesh* ConvertToIsoSurfaceMesh( const Mesh& sourceMesh, f32 drawingDistance, int displayedLayer, IsoSurfaceSamplingCache* samplingCache,
                               MeshPool* meshPool, const TemporaryMemory& tmpMemory, RenderCommands* renderCommands )
{
    // Make bounds same length on all axes
    v3 const& centerP = sourceMesh.bounds.center;
    v3 dim = Dim( sourceMesh.bounds );
    f32 cubeSide = Max( dim.x, Max( dim.y, dim.z ) );

    aabb bounds = AABBCenterSize( centerP, cubeSide );
    v3 boundsMin, boundsMax;
    MinMax( bounds, &boundsMin, &boundsMax );

    ASSERT( samplingCache->cellsPerAxis.x == samplingCache->cellsPerAxis.y );
    int cellsPerAxis = samplingCache->cellsPerAxis.x;
    int gridLinesPerAxis = cellsPerAxis + 1;
    f32 step = cubeSide / cellsPerAxis;

    // We store all intersections with grid rays in a hashtable where encoded integer coords are the hash
    // For example, for X rays, the Y|Z coords are the hash, for Y rays, the X|Z coords, etc.
    // NOTE This can be heavily compressed if needed by using a more compact hash, since most entries will be empty anyway
    int rayCount = gridLinesPerAxis * gridLinesPerAxis;       // Must be power of 2
    Array<Hit> gridHits( tmpMemory.arena, 100000, Temporary() );

    RenderSetShader( ShaderProgramName::PlainColor, renderCommands );
    RenderSetMaterial( nullptr, renderCommands );

    // Get all triangles and find their bounds
    int triangleCount = sourceMesh.indices.count / 3;
    ASSERT( triangleCount * 3 == sourceMesh.indices.count );

    int vertIndex = 0;
    for( int i = 0; i < triangleCount; ++i )
    {
        TexturedVertex const& v0 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        TexturedVertex const& v1 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        TexturedVertex const& v2 = sourceMesh.vertices[sourceMesh.indices[vertIndex++]];
        tri t = { v0.p, v1.p, v2.p };

        v3 triBoundsMin =
        {
            Min( v0.p.x, Min( v1.p.x, v2.p.x ) ),
            Min( v0.p.y, Min( v1.p.y, v2.p.y ) ),
            Min( v0.p.z, Min( v1.p.z, v2.p.z ) ),
        };
        v3 triBoundsMax = 
        {
            Max( v0.p.x, Max( v1.p.x, v2.p.x ) ),
            Max( v0.p.y, Max( v1.p.y, v2.p.y ) ),
            Max( v0.p.z, Max( v1.p.z, v2.p.z ) ),
        };

        // Test each tri for intersection against all marching grid rays (early out using bounds first)
        // (this could be accelerated easily by using a binary search on grid coords)
        
        // Y rays
        for( int x = 0; x < gridLinesPerAxis; ++x )
        {
            f32 xGrid = boundsMin.x + x * step;

            if( triBoundsMax.x < xGrid )
                break;
            if( xGrid >= triBoundsMin.x )
            {
                for( int z = 0; z < gridLinesPerAxis; ++z )
                {
                    f32 zGrid = boundsMin.z + z * step;

                    if( triBoundsMax.z < zGrid )
                        break;
                    if( zGrid >= triBoundsMin.z )
                    {
                        ray r = { { xGrid, boundsMin.y, zGrid }, { 0, 1, 0 } };
                        v3 pI = V3Zero;

                        // NOTE Rays intersecting at the very edges of tris can be a problem at finer resolutions
                        // (not easily solved even when adding a manual tolerance)
                        if( Intersects( r, t, &pI ) )
                        {
                            // Store intersection coords relative to grid
                            f32 hitCoord = (pI - boundsMin).y;
                            gridHits.Push( { V2i( x, z ), hitCoord } );
                        }
                    }
                }
            }
        }
    }

    ClearScratchBuffers( meshPool );
    bool firstLayer = true;

    for( int k = 0; k < cellsPerAxis; ++k )
    {
        f32* bottomSamples = samplingCache->bottomLayerSamples;
        f32* topSamples = samplingCache->topLayerSamples;

        // Pre-sample top and bottom slices of values for each layer
        // so we only sample one corner per cube instead of 8
        for( int n = 0; n < 2; ++n )
        {
            if( n == 0 && !firstLayer )
                continue;

            f32* sampledLayer = n ? topSamples : bottomSamples;
            f32* sample = sampledLayer;
            for( int i = 0; i < gridLinesPerAxis; ++i )
            {
                v2i vIK = n == 0 ? V2i( i, k ) : V2i( i, k+1 );

                // FIXME
                ARRAY(Hit, 10, rayHits);
                FilterHits( gridHits, vIK, &rayHits );

                // This usually signals a tolerance error in Intersects() (or a bogus mesh, of course)
                ASSERT( (rayHits.count & 0x1) == 0 );

                int h = 0;
                int value = 1;
                for( int j = 0; j < gridLinesPerAxis; ++j )
                {
                    f32 yGrid = j * step;
                    while( h < rayHits.count && rayHits[h].hitCoord < yGrid )
                    {
                        value *= -1;
                        h++;
                    }

                    *sample++ = (f32)value;
                }
            }
        }

        // Keep a cache of already calculated vertices to eliminate duplication
        ClearVertexCaches( samplingCache, firstLayer );

        for( int i = 0; i < cellsPerAxis; ++i )
        {
            for( int j = 0; j < cellsPerAxis; ++j )
            {
                v3 p = boundsMin + V3i( i, j, k ) * step;

#if 1
                if( displayedLayer == k )
                {
                    u32 color = Pack01ToRGBA( 0, 1, 0, 1 );
                    f32 value = samplingCache->bottomLayerSamples[i * gridLinesPerAxis + j];
                    //if( value < 0 )
                        //RenderBoundsAt( p, 0.005f * drawingDistance, color, renderCommands );

                    value = samplingCache->topLayerSamples[i * gridLinesPerAxis + j];
                    if( value < 0 )
                        RenderBoundsAt( p + V3( 0.f, 0.f, step ), 0.005f * drawingDistance, color, renderCommands );
                }
#endif

                MarchCube( p, V2i( i, j ), V2i( cellsPerAxis ), step, samplingCache,
                           &meshPool->scratchVertices, &meshPool->scratchIndices );
            }
        }

        firstLayer = false;
        SwapTopAndBottomLayers( samplingCache );
    }

    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    result->bounds = bounds;
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
    f32 dUp = Max( Dot( vUp, p ), Dot( -vUp, p ) ) - path->thicknessSq;
    f32 dRight = Max( Dot( vRight, p ), Dot( -vRight, p ) ) - path->thicknessSq;
 
    f32 result = Max( dUp, dRight );

    // Turns
    if( path->nextBasis )
    {
        // Cut with plane across motion dir
        // NOTE For now we're gonna assume that all turns take place at the area's center point
        f32 d = 0; // path->areaSideMeters/2 - path->distanceToNextTurn;
        result += Clamp0( Dot( vForward, p ) + d );

        vRight = GetBasisX( *path->nextBasis );
        vForward = GetBasisY( *path->nextBasis );
        vUp = GetBasisZ( *path->nextBasis );

        dUp = Max( Dot( vUp, p ), Dot( -vUp, p ) ) - path->thicknessSq;
        dRight = Max( Dot( vRight, p ), Dot( -vRight, p ) ) - path->thicknessSq;

        f32 newResult = Max( dUp, dRight );
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

        f32 newResult = Max( dUp, dRight );
        // Cut with plane across motion dir (bakcwards)
        // NOTE For now we're gonna assume that all forks take place at the area's center point
        f32 d = 0; // path->areaSideMeters/2 - path->distanceToNextFork;
        newResult += Clamp0( Dot( -vForward, p ) + d );

        // NOTE Min() means 'union'
        result = Min( result, newResult );
    }

    // FIXME Use an offset that's in proportion to step resolution (add resolution to path struct?)
    return result + 0.1f;
}

internal f32
SampleCylinder( const void* samplingData, const v3& p )
{
    MeshGeneratorPathData* path = (MeshGeneratorPathData*)samplingData;
    v3 vForward = GetBasisY( path->basis );

    v3 vP = p - path->pCenter;
    // l is the length of the projection of vP along the director line
    f32 l = Dot( vForward, vP );
    // Translate p the previous distance along vDir (backwards) to align it with pCenter
    v3 pPrime = p - vForward * l;

    f32 result = DistanceSq( path->pCenter, pPrime ) - path->thicknessSq;
    return result;
}

u32
GenerateOnePathStep( MeshGeneratorPathData* path, f32 resolutionMeters, bool advancePosition,
                     IsoSurfaceSamplingCache* samplingCache,
                     MeshPool* meshPool, Mesh** outMesh, MeshGeneratorPathData* nextFork )
{
    bool turnInThisStep = path->distanceToNextTurn < path->areaSideMeters;
    bool forkInThisStep = path->distanceToNextFork < path->areaSideMeters;

    f32 pi2 = PI / 2;
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
            path->distanceToNextTurn = RandomRangeF32( path->minDistanceToTurn, path->maxDistanceToTurn );
        }
        if( forkInThisStep )
        {
            path->distanceToNextFork = RandomRangeF32( path->minDistanceToFork, path->maxDistanceToFork );
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
    f32 result = Max( Max( d.x, d.y ), d.z );
    return result;
}

#if 0
MESH_GENERATOR_FUNC(MeshGeneratorRoomFunc)
{
    Mesh* result = MarchVolumeFast( { V3Zero, V3iZero }, V3( generatorData.areaSideMeters ), generatorData.resolutionMeters,
                                    SampleRoomBody, &generatorData.room, samplingCache, vertices, indices );
    result->mTransform = M4Translation( entityCoords.relativeP );

    return result;
}
#endif



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
    i32 edgeVertexOffsets[12][2] =
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
