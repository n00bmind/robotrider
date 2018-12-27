MarchingCacheBuffers
InitMarchingCacheBuffers( MemoryArena* arena, u32 cellsPerAxis )
{
    MarchingCacheBuffers result;
    result.cellsPerAxis = cellsPerAxis;

    // NOTE We add an extra row of cells at the ends, which simplifies the algorithm by eliminating edge cases
    u32 stepsPerAxis = cellsPerAxis + 1;
    u32 layerCellCount = stepsPerAxis * stepsPerAxis;

    result.bottomLayerSamples = PUSH_ARRAY( arena, layerCellCount, r32 );
    result.topLayerSamples = PUSH_ARRAY( arena, layerCellCount, r32 );
    result.bottomLayerVertexIndices = PUSH_ARRAY( arena, layerCellCount * 2, u32 );
    result.middleLayerVertexIndices = PUSH_ARRAY( arena, layerCellCount, u32 );
    result.topLayerVertexIndices = PUSH_ARRAY( arena, layerCellCount * 2, u32 );

    return result;
}

internal void
ClearVertexCaches( MarchingCacheBuffers* buffers, bool clearBottomLayer )
{
    u32 stepsPerAxis = buffers->cellsPerAxis + 1;
    u32 layerCellCount = stepsPerAxis * stepsPerAxis;

    if( clearBottomLayer )
    {
        SET( buffers->bottomLayerVertexIndices, U32MAX,
             layerCellCount * 2 * sizeof(u32) );
    }
    SET( buffers->middleLayerVertexIndices, U32MAX,
         layerCellCount * sizeof(u32) );
    SET( buffers->topLayerVertexIndices, U32MAX, 
         layerCellCount * 2 * sizeof(u32) );
}

internal void
SwapTopAndBottomLayers( MarchingCacheBuffers* buffers )
{
    MarchingCacheBuffers copy = *buffers;
    buffers->bottomLayerSamples = copy.topLayerSamples;
    buffers->topLayerSamples = copy.bottomLayerSamples;
    buffers->bottomLayerVertexIndices = copy.topLayerVertexIndices;
    buffers->topLayerVertexIndices = copy.bottomLayerVertexIndices;
}


void
Init( MeshPool* pool, MemoryArena* arena, sz size )
{
    new (&pool->scratchVertices) BucketArray<TexturedVertex>( arena, 1024 );
    new (&pool->scratchIndices) BucketArray<u32>( arena, 1024 );

    // Initialize empty sentinel
    pool->memorySentinel.prev = &pool->memorySentinel;
    pool->memorySentinel.next = &pool->memorySentinel;
    pool->memorySentinel.size = 0;
    pool->memorySentinel.flags = MemoryBlockFlags::None;

    // Insert empty block with the whole memory chunk
    InsertBlock( &pool->memorySentinel, size, PUSH_SIZE( arena, size ) );

    pool->meshCount = 0;
}

void
ClearScratchBuffers( MeshPool* pool )
{
    pool->scratchVertices.Clear();
    pool->scratchIndices.Clear();
}

Mesh*
AllocateMesh( MeshPool* pool, u32 vertexCount, u32 indexCount )
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

        Init( result );
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

Mesh*
AllocateMeshFromScratchBuffers( MeshPool* pool )
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

void
ReleaseMesh( Mesh** mesh )
{
    // TODO Move the sentinel in MeshPool to a 'parent' MemoryPool and add a reference to that
    // in the MemoryBlock so we can remove the ownerPool thing!
    ReleaseBlockAt( *mesh, &(*mesh)->ownerPool->memorySentinel );
    (*mesh)->ownerPool->meshCount--;
    *mesh = nullptr;
}

///// MARCHING CUBES /////

typedef float MarchedCubeSampleFunc( const void* sampleData, const v3& p );

struct VertexCacheIndex
{
    // Offset to the cell entry 'owning' the corresponding edge
    v2i vCellOffset;
    // Which table, 0 - bottom, 1 - middle (vertical), 2 - top
    u16 cacheTableIndex;
    // Additional offset in table, for bottom & top only, 0 - X axis, 1 - Y axis
    u16 cacheTableOffset;
};

// HACK To allow forward-declaring internal arrays while still keeping them internal
// C++ is wonderful
namespace
{
    extern v3i cornerOffsets[8];
    extern u32 edgeVertexOffsets[12][2];
    extern VertexCacheIndex vertexCacheIndices[12];
    extern int triangleTable[][16];
}

internal void
MarchCube( const v3& pOrigin, r32 cubeSize,
           const v2i& pLayerOrigin, MarchingCacheBuffers* cacheBuffers, u32 layerStepsCount,
           BucketArray<TexturedVertex>* vertices, BucketArray<u32>* indices )
{
    TIMED_BLOCK;

    v3i pLayerOrigin3D = V3i( pLayerOrigin.x, pLayerOrigin.y, 0 );

    // Construct case mask from 8 corner samples
    u32 caseIndex = 0;
    r32 cornerSamples[8];
    for( int i = 0; i < 8; ++i )
    {
        v3i cornerOffset = cornerOffsets[i];
        v3i pLayer = pLayerOrigin3D + cornerOffset;
        u32 layerOffset = pLayer.x * layerStepsCount + pLayer.y;

        r32 sample = cornerOffset.z
            ? cacheBuffers->topLayerSamples[layerOffset]
            : cacheBuffers->bottomLayerSamples[layerOffset];

        if( sample >= 0 )
            caseIndex |= 1 << i;

        cornerSamples[i] = sample;
    }

    // Early out if entirely inside or outside
    if( caseIndex == 0 || caseIndex == 0xFF )
        return;

    u32* vertexCaches[3] =
    {
        cacheBuffers->bottomLayerVertexIndices,
        cacheBuffers->middleLayerVertexIndices,
        cacheBuffers->topLayerVertexIndices,
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

            v2i pLayer = pLayerOrigin + idx.vCellOffset;
            u32 vertexCacheOffset = pLayer.x * layerStepsCount + pLayer.y;
            if( idx.cacheTableIndex != 1)
            {
                // Top or bottom layers
                vertexCacheOffset = 2 * vertexCacheOffset + idx.cacheTableOffset;
            }

#if 1       // Use vertex cache (produces around 7x less vertices)
            u32 cachedVertexIndex = vertexCache[vertexCacheOffset];
            if( cachedVertexIndex != U32MAX )
            {
                indices->Add( cachedVertexIndex );
            }
            else
#endif
            {
                // No cached vertex, so go on and create one
                u32 indexA = edgeVertexOffsets[edgeCaseIndex][0];   // Edge start
                u32 indexB = edgeVertexOffsets[edgeCaseIndex][1];   // Edge end

                v3 pA = pOrigin + V3( cornerOffsets[indexA] ) * cubeSize;
                v3 pB = pOrigin + V3( cornerOffsets[indexB] ) * cubeSize;

#if 0           // Non interpolated version (DMC)

                v3 vPos = (pA + pB) / 2;
#else
                // Interpolate along the edge
                float sA = cornerSamples[indexA];
                float sB = cornerSamples[indexB];

                // TODO Look into implementing SnapMC to eliminate any degenerate and weird triangles
                // which also means less tris to render!
                // http://web.cse.ohio-state.edu/~wenger.4/publications/isomesh.pdf
                float diff = sA - sB;
                // In case sampled function is the same, arbitrarily use midpoint
                float t = AlmostEqual( diff, 0.f ) ? 0.5f : sA / diff;
                v3 vPos = pA + ((pB - pA) * t);
#endif

                // Add new vertex to the mesh
                u32 newVertexIndex = vertices->count;
                indices->Add( newVertexIndex );
                TexturedVertex v = {};
                v.p = vPos;
                v.color = Pack01ToRGBA( 1, 1, 1, 1 );
                vertices->Add( v );

                // Cache it too!
                vertexCache[vertexCacheOffset] = newVertexIndex;
            }

            ++caseVertex;
        }
    }
}

Mesh*
MarchAreaFast( const v3& pCenter, r32 areaSideMeters, r32 cubeSizeMeters, MarchedCubeSampleFunc* sampleFunc, const void* sampleData,
               MarchingCacheBuffers* cacheBuffers, MeshPool* meshPool )
{
    ClearScratchBuffers( meshPool );

    {
        TIMED_BLOCK;

        u32 cellsPerAxis = (u32)(areaSideMeters / cubeSizeMeters);
        ASSERT( cacheBuffers->cellsPerAxis == cellsPerAxis );
        r32 halfSideMeters = areaSideMeters / 2;
        v3 cornerOffset = { -halfSideMeters, -halfSideMeters, -halfSideMeters };

        v3 vXDelta = V3( cubeSizeMeters, 0, 0 );
        v3 vYDelta = V3( 0, cubeSizeMeters, 0 );

        u32 gridLinesPerAxis = cellsPerAxis + 1;
        bool firstLayer = true;

        // Iterate cells when going vertically, since we consider both the bottom and the top of the cubes on each pass
        for( u32 k = 0; k < cellsPerAxis; ++k )
        {
            r32* bottomSamples = cacheBuffers->bottomLayerSamples;
            r32* topSamples = cacheBuffers->topLayerSamples;

            // Pre-sample top and bottom slices of values for each layer
            // so we only sample one corner per cube instead of 8
            for( int n = 0; n < 2; ++n )
            {
                if( n == 0 && !firstLayer )
                    continue;

                r32* sampledLayer = n ? topSamples : bottomSamples;

                v3 p = pCenter + cornerOffset + V3i( 0, 0, k + n ) * cubeSizeMeters;

                r32* sample = sampledLayer;
                for( u32 i = 0; i < gridLinesPerAxis; ++i )
                {
                    v3 pAtRowStart = p;
                    for( u32 j = 0; j < gridLinesPerAxis; ++j )
                    {
                        *sample++ = sampleFunc( sampleData, p );
                        p += vYDelta;
                    }
                    p = pAtRowStart + vXDelta;
                }
            }

            // Keep a cache of already calculated vertices to eliminate duplication
            ClearVertexCaches( cacheBuffers, firstLayer );

            v3 p = pCenter + cornerOffset + V3i( 0, 0, k ) * cubeSizeMeters;
            for( u32 i = 0; i < cellsPerAxis; ++i )
            {
                v3 pAtRowStart = p;
                for( u32 j = 0; j < cellsPerAxis; ++j )
                {
                    MarchCube( p, cubeSizeMeters, V2i( i, j ), cacheBuffers, gridLinesPerAxis,
                               &meshPool->scratchVertices, &meshPool->scratchIndices );
                    p += vYDelta;
                }
                p = pAtRowStart + vXDelta;
            }

            firstLayer = false;
            SwapTopAndBottomLayers( cacheBuffers );
        }
    }

    // Write output mesh
    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    return result;
}

internal r32
SampleMetaballs( const void* sampleData, const v3& pos )
{
    const Array<Metaball>& balls = *(const Array<Metaball>*)sampleData;

    r32 minValue = R32MAX;
    for( u32 i = 0; i < balls.maxCount; ++i )
    {
        r32 value = DistanceSq( balls[i].pCenter, pos ) - balls[i].radiusMeters;
        if( value < minValue )
            minValue = value;
    }

    return minValue;
}

void
TestMetaballs( float areaSideMeters, float cubeSizeMeters, float elapsedT, MarchingCacheBuffers* cacheBuffers,
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
    Mesh* metaMesh = MarchAreaFast( V3Zero, areaSideMeters, cubeSizeMeters,
                                    SampleMetaballs, &balls,
                                    cacheBuffers, meshPool );

    PushMesh( *metaMesh, renderCommands );
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
CalculateError( InflatedMesh* mesh, u32 v1Idx, u32 v2Idx, v3* result )
{
    InflatedVertex& v1 = mesh->vertices[v1Idx];
    InflatedVertex& v2 = mesh->vertices[v2Idx];

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
UpdateMesh( InflatedMesh* mesh, Array<InflatedVertexRef>* refs, u32 iteration, const TemporaryMemory& tmpMemory )
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
            InflatedTriangle& tri = mesh->triangles[i];
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
            InflatedTriangle& tri = mesh->triangles[i];
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
        InflatedTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
            mesh->vertices[tri.v[j]].refCount++;
    }

    u32 refStart = 0;
    for( u32 i = 0; i < mesh->vertices.count; ++i )
    {
        InflatedVertex& v = mesh->vertices[i];
        v.refStart = refStart;
        refStart += v.refCount;
        v.refCount = 0;
    }

    refs->count = mesh->triangles.count * 3;
    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        InflatedTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
        {
            InflatedVertex& v = mesh->vertices[tri.v[j]];
            InflatedVertexRef& r = (*refs)[v.refStart + v.refCount];

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

            InflatedVertex& v = mesh->vertices[i];

            for( u32 j = 0; j < v.refCount; ++j )
            {
                u32 tId = (*refs)[v.refStart + j].tId;
                InflatedTriangle& tri = mesh->triangles[tId];

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
Flipped( const InflatedMesh& mesh, const Array<InflatedVertexRef>& refs,
         const v3& p, u32 i0, u32 i1, const InflatedVertex& v0, const InflatedVertex& v1, Array<bool>* deleted )
{
    for( u32 k = 0; k < v0.refCount; ++k )
    {
        const InflatedTriangle& tri = mesh.triangles[refs[v0.refStart + k].tId];
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
UpdateTriangles( u32 i0, const InflatedVertex& v, const Array<bool>& deleted,
                 InflatedMesh* mesh, Array<InflatedVertexRef>* refs, u32* deleteTriangleCount )
{
    v3 p;
    for( u32 k = 0; k < v.refCount; ++k )
    {
        InflatedVertexRef& ref = (*refs)[v.refStart + k];
        InflatedTriangle& tri = mesh->triangles[ref.tId];

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
CompactMesh( InflatedMesh* mesh )
{
    u32 dst = 0;

    for( u32 i = 0; i < mesh->vertices.count; ++i )
        mesh->vertices[i].refCount = 0;

    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        InflatedTriangle& tri = mesh->triangles[i];
        if( !tri.deleted )
        {
            mesh->triangles[dst++] = tri;
            for( int j = 0; j < 3; ++j )
                // TODO This looks wrong!?
                mesh->vertices[tri.v[j]].refCount = 1;
        }
    }
    mesh->triangles.count = dst;
    dst = 0;

    for( u32 i = 0; i < mesh->vertices.count; ++i )
    {
        InflatedVertex& v = mesh->vertices[i];
        if( v.refCount )
        {
            v.refStart = dst;
            mesh->vertices[dst].p = v.p;
            dst++;
        }
    }

    for( u32 i = 0; i < mesh->triangles.count; ++i )
    {
        InflatedTriangle& tri = mesh->triangles[i];
        for( int j = 0; j < 3; ++j )
            tri.v[j] = mesh->vertices[tri.v[j]].refStart;
    }
    mesh->vertices.count = dst;
}

// Taken from https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification
void FastQuadricSimplify( InflatedMesh* mesh, u32 targetTriCount, const TemporaryMemory& tmpMemory,
                          r32 agressiveness = 7 )
{
    Array<InflatedVertexRef> refs( tmpMemory.arena, 0, 500000 );

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
            InflatedTriangle& tri = mesh->triangles[i];
            if( tri.error[3] > threshold || tri.deleted || tri.dirty )
                continue;

            for( int j = 0; j < 3; ++j )
            {
                if( tri.error[j] < threshold )
                {
                    Array<bool> deleted0( tmpMemory.arena, 0, 1000 );
                    Array<bool> deleted1( tmpMemory.arena, 0, 1000 );

                    u32 i0 = tri.v[j];          InflatedVertex& v0 = mesh->vertices[i0];
                    u32 i1 = tri.v[(j+1)%3];    InflatedVertex& v1 = mesh->vertices[i1];

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
                            memcpy( &refs[v0.refStart], &refs[refStart], refCount * sizeof(InflatedVertexRef) );
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

Mesh*
ConvertToIsoSurfaceMesh( const Mesh& sourceMesh, MarchingCacheBuffers* cacheBuffers, MeshPool* meshPool,
                         const TemporaryMemory& tmpMemory, RenderCommands* renderCommands,
                         const EditorState& editorState )
{
    // Make bounds same length on all axes
    r32 xDim = sourceMesh.bounds.xMax - sourceMesh.bounds.xMin;
    r32 yDim = sourceMesh.bounds.yMax - sourceMesh.bounds.yMin;
    r32 zDim = sourceMesh.bounds.zMax - sourceMesh.bounds.zMin;

    v3 pCenter = { sourceMesh.bounds.xMin + xDim / 2, sourceMesh.bounds.yMin + yDim / 2, sourceMesh.bounds.zMin + zDim / 2 };
    r32 dim = Max( xDim, Max( yDim, zDim ) );

    aabb box =
    {
        pCenter.x - dim / 2, pCenter.x + dim / 2,
        pCenter.y - dim / 2, pCenter.y + dim / 2,
        pCenter.z - dim / 2, pCenter.z + dim / 2,
    };
    v3 pGridOrigin = { box.xMin, box.yMin, box.zMin };

    u32 cellsPerAxis = cacheBuffers->cellsPerAxis;
    u32 gridLinesPerAxis = cellsPerAxis + 1;
    r32 step = dim / cellsPerAxis;

    // We store all intersections with grid rays in a hashtable where encoded integer coords are the hash
    // For example, for X rays, the Y|Z coords are the hash, for Y rays, the X|Z coords, etc.
    // NOTE This can be heavily compressed if needed by using a more compact hash, since most entries will be empty anyway
    u32 rayCount = gridLinesPerAxis * gridLinesPerAxis;       // Must be power of 2
    Array<Hit> gridHits( tmpMemory.arena, 0, 100000 );

    PushProgramChange( ShaderProgramName::PlainColor, renderCommands );
    PushMaterial( nullptr, renderCommands );

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

        aabb triBounds;
        triBounds.xMin = Min( v0.p.x, Min( v1.p.x, v2.p.x ) );
        triBounds.xMax = Max( v0.p.x, Max( v1.p.x, v2.p.x ) );
        triBounds.yMin = Min( v0.p.y, Min( v1.p.y, v2.p.y ) );
        triBounds.yMax = Max( v0.p.y, Max( v1.p.y, v2.p.y ) );
        triBounds.zMin = Min( v0.p.z, Min( v1.p.z, v2.p.z ) );
        triBounds.zMax = Max( v0.p.z, Max( v1.p.z, v2.p.z ) );

        // Test each tri for intersection against all marching grid rays (early out using bounds first)
        // (this could be accelerated easily by using a binary search on grid coords)
        
        // Y rays
        for( u32 x = 0; x < gridLinesPerAxis; ++x )
        {
            r32 xGrid = box.xMin + x * step;

            if( triBounds.xMax < xGrid )
                break;
            if( xGrid >= triBounds.xMin )
            {
                for( u32 z = 0; z < gridLinesPerAxis; ++z )
                {
                    r32 zGrid = box.zMin + z * step;

                    if( triBounds.zMax < zGrid )
                        break;
                    if( zGrid >= triBounds.zMin )
                    {
                        ray r = { { xGrid, box.yMin, zGrid }, { 0, 1, 0 } };
                        v3 pI = V3Zero;

                        // NOTE Rays intersecting at the very edges of tris can be a problem at finer resolutions
                        // (not easily solved even when adding a manual tolerance)
                        if( Intersects( r, t, &pI ) )
                        {
                            // Store intersection coords relative to grid
                            r32 hitCoord = (pI - pGridOrigin).y;
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
        r32* bottomSamples = cacheBuffers->bottomLayerSamples;
        r32* topSamples = cacheBuffers->topLayerSamples;

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
        ClearVertexCaches( cacheBuffers, firstLayer );

        for( u32 i = 0; i < cellsPerAxis; ++i )
        {
            for( u32 j = 0; j < cellsPerAxis; ++j )
            {
                v3 p = pGridOrigin + V3i( i, j, k ) * step;

#if 1
                if( editorState.displayedLayer == k )
                {
                    u32 color = Pack01ToRGBA( 0, 1, 0, 1 );
                    r32 value = cacheBuffers->bottomLayerSamples[i * gridLinesPerAxis + j];
                    //if( value < 0 )
                        //DrawBoxAt( p, 0.005f * editorState.drawingDistance, color, renderCommands );

                    value = cacheBuffers->topLayerSamples[i * gridLinesPerAxis + j];
                    if( value < 0 )
                        DrawBoxAt( p + V3( 0, 0, step ), 0.005f * editorState.drawingDistance, color, renderCommands );
                }
#endif

                MarchCube( p, step, V2i( i, j ), cacheBuffers, gridLinesPerAxis,
                           &meshPool->scratchVertices, &meshPool->scratchIndices );
            }
        }

        firstLayer = false;
        SwapTopAndBottomLayers( cacheBuffers );
    }

    Mesh* result = AllocateMeshFromScratchBuffers( meshPool );
    result->bounds = box;
    return result;
}


///// MESH GENERATION /////

internal r32
SampleCuboid( const void* sampleData, const v3& p )
{
    GeneratorPathData* path = (GeneratorPathData*)sampleData;
    v3 vRight = GetXBasis( path->basis );
    v3 vForward = GetYBasis( path->basis );
    v3 vUp = GetZBasis( path->basis );

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

        vRight = GetXBasis( *path->nextBasis );
        vForward = GetYBasis( *path->nextBasis );
        vUp = GetZBasis( *path->nextBasis );

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
        vRight = GetXBasis( path->nextFork->basis );
        vForward = GetYBasis( path->nextFork->basis );
        vUp = GetZBasis( path->nextFork->basis );

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
SampleCylinder( const void* sampleData, const v3& p )
{
    GeneratorPathData* path = (GeneratorPathData*)sampleData;
    v3 vForward = GetYBasis( path->basis );

    v3 vP = p - path->pCenter;
    // l is the length of the projection of vP along the director line
    r32 l = Dot( vForward, vP );
    // Translate p the previous distance along vDir (backwards) to align it with pCenter
    v3 pPrime = p - vForward * l;

    r32 result = DistanceSq( path->pCenter, pPrime ) - path->thicknessSq;
    return result;
}

u32
GenerateOnePathStep( GeneratorPathData* path, r32 resolutionMeters, bool advancePosition,
                     MarchingCacheBuffers* cacheBuffers,
                     MeshPool* meshPool, Mesh** outMesh, GeneratorPathData* nextFork )
{
    bool turnInThisStep = path->distanceToNextTurn < path->areaSideMeters;
    bool forkInThisStep = path->distanceToNextFork < path->areaSideMeters;

    r32 pi2 = PI / 2;
    m4 rotations[4] =
    {
        // Expressed as if +Y == 'forward'
        ZRotation(  pi2 ),
        ZRotation( -pi2 ),
        XRotation(  pi2 ),
        XRotation( -pi2 ),
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
                              SampleCuboid, path, cacheBuffers, meshPool );
    (*outMesh)->mTransform = Translation( path->pCenter );

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

        v3 vForward = GetYBasis( path->basis );
        path->pCenter += vForward * (path->areaSideMeters + 0.1f);
    }

    return forkInThisStep ? 1 : 0;
}

internal r32
SampleHullNode( const void* sampleData, const v3& p )
{
    TIMED_BLOCK;

    GeneratorHullNodeData* genData = (GeneratorHullNodeData*)sampleData;

    // Just a sphere for now
    r32 result = DistanceSq( p, V3Zero ) - 5;
    return result;
}

GENERATOR_FUNC(GeneratorHullNodeFunc)
{
    const GeneratorHullNodeData* hullGenData = (const GeneratorHullNodeData*)generatorData;

    r32 areaSideMeters = hullGenData->areaSideMeters;
    r32 resolutionMeters = hullGenData->resolutionMeters;

    Mesh* result = MarchAreaFast( V3Zero, areaSideMeters, resolutionMeters,
                                  SampleHullNode, generatorData,
                                  cacheBuffers, meshPool );
    result->mTransform = Translation( p.pClusterOffset );

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
        { 0, 1 },
        { 1, 2 },
        { 3, 2 },
        { 0, 3 },
        { 4, 5 },
        { 5, 6 },
        { 7, 6 },
        { 4, 7 },
        { 0, 4 },
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

    // List of vertex indices for every triangle & for every possible case
    // Up to 15 vertices per case, -1 indicates end of sequence
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
