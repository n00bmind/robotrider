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



#define PUSH_RENDER_ELEMENT(commands, type) (type *)_PushRenderElement( commands, sizeof(type), RenderEntryType::type )
internal RenderEntry *
_PushRenderElement( RenderCommands *commands, u32 size, RenderEntryType type )
{
    RenderEntry *result = 0;
    RenderBuffer &buffer = commands->renderBuffer;

    if( buffer.size + size < buffer.maxSize )
    {
        result = (RenderEntry *)(buffer.base + buffer.size);
        ZERO( result, size );
        result->type = type;
        result->size = size;
        buffer.size += size;
    }
    else
    {
        // We don't just do an assert so we're fail tolerant in release builds
        INVALID_CODE_PATH
    }

    // NOTE Reset all batched entries so they start over as needed
    // TODO We will probably need more control over this in the future
    commands->currentTris = nullptr;
    commands->currentLines = nullptr;

    return result;
}

void
RenderClear( const v4& color, RenderCommands *commands )
{
    RenderEntryClear *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryClear );
    if( entry )
    {
        entry->color = color;
    }
}

internal RenderEntryTexturedTris *
GetOrCreateCurrentTris( RenderCommands *commands )
{
    // TODO Artificially break the current bundle when we get to an estimated "optimum size"
    // (if there is such a thing.. I read somewhere that was 1 to 4 megs?)
    if( !commands->currentTris )
    {
        commands->currentTris = PUSH_RENDER_ELEMENT( commands, RenderEntryTexturedTris );
        commands->currentTris->vertexBufferOffset = commands->vertexBuffer.count;
        commands->currentTris->indexBufferOffset = commands->indexBuffer.count;
        commands->currentTris->vertexCount = 0;
        commands->currentTris->indexCount = 0;
    }

    RenderEntryTexturedTris *result = commands->currentTris;
    return result;
}

internal RenderEntryLines *
GetOrCreateCurrentLines( RenderCommands *commands )
{
    if( !commands->currentLines )
    {
        commands->currentLines = PUSH_RENDER_ELEMENT( commands, RenderEntryLines );
        commands->currentLines->vertexBufferOffset = commands->vertexBuffer.count;
        commands->currentLines->lineCount = 0;
    }

    RenderEntryLines *result = commands->currentLines;
    return result;
}

// TODO Force inline
inline internal void
PushVertex( const v3 &p, u32 color, const v2 &uv, RenderCommands *commands )
{
    //TIMED_BLOCK;

    ASSERT( commands->vertexBuffer.count + 1 <= commands->vertexBuffer.maxCount );

    TexturedVertex *vert = commands->vertexBuffer.base + commands->vertexBuffer.count;
    vert->p = p;
    vert->color = color;
    vert->uv = uv;

    commands->vertexBuffer.count++;
}

inline internal void
PushIndex( u32 value, RenderCommands* commands )
{
    //TIMED_BLOCK;

    ASSERT( commands->indexBuffer.count + 1 <= commands->indexBuffer.maxCount );

    u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
    *index = value;

    commands->indexBuffer.count++;
}

// Push 4 vertices (1st vertex is "top-left" and counter-clockwise from there)
void
RenderQuad( const v3 &p1, const v3 &p2, const v3 &p3, const v3 &p4, u32 color, RenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        int indexOffsetStart = entry->vertexCount;

        PushVertex( p1, color, { 0, 0 }, commands );
        PushVertex( p2, color, { 0, 1 }, commands );
        PushVertex( p3, color, { 1, 1 }, commands );
        PushVertex( p4, color, { 1, 0 }, commands );
        entry->vertexCount += 4;

        // Push 6 indices for vertices 0-1-2 & 2-3-0
        PushIndex( indexOffsetStart + 0, commands );
        PushIndex( indexOffsetStart + 1, commands );
        PushIndex( indexOffsetStart + 2, commands );

        PushIndex( indexOffsetStart + 2, commands );
        PushIndex( indexOffsetStart + 3, commands );
        PushIndex( indexOffsetStart + 0, commands );
        entry->indexCount += 6;
    }
}

void
RenderLine( v3 pStart, v3 pEnd, u32 color, RenderCommands *commands )
{
    RenderEntryLines *entry = GetOrCreateCurrentLines( commands );
    if( entry )
    {
        PushVertex( pStart, color, { 0, 0 }, commands );
        PushVertex( pEnd, color, { 0, 0 }, commands );

        ++entry->lineCount;
    }
}

void
RenderSetShader( ShaderProgramName programName, RenderCommands *commands )
{
    RenderEntryProgramChange *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryProgramChange );
    if( entry )
    {
        entry->programName = programName;
    }
}

void
RenderSetMaterial( Material* material, RenderCommands* commands )
{
    RenderEntryMaterial *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryMaterial );
    if( entry )
    {
        entry->material = material;
    }
}

void
RenderSwitch( RenderSwitch renderSwitch, bool enable, RenderCommands* commands )
{
    RenderEntrySwitch *entry = PUSH_RENDER_ELEMENT( commands, RenderEntrySwitch );
    if( entry )
    {
        entry->renderSwitch = renderSwitch;
        entry->enable = enable;
    }
}



void
RenderMesh( const Mesh& mesh, RenderCommands *commands )
{
    TIMED_BLOCK;

    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
#if 0
        for( u32 i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& v = mesh.vertices[i];

            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( mesh.mTransform * v.p, v.color, v.uv, commands );
        }
        int indexOffsetStart = entry->vertexCount;
        entry->vertexCount += mesh.vertexCount;

        for( u32 i = 0; i < mesh.indexCount; ++i )
        {
            PushIndex( indexOffsetStart + mesh.indices[i], commands );
        }
        entry->indexCount += mesh.indexCount;
#else
        ASSERT( commands->vertexBuffer.count + mesh.vertexCount <= commands->vertexBuffer.maxCount );

        //const __m128 r0 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 0 );
        //const __m128 r1 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 4 );
        //const __m128 r2 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 8 );
        for( u32 i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& src = mesh.vertices[i];
            TexturedVertex *dst = commands->vertexBuffer.base + commands->vertexBuffer.count;

            // TODO Redesign Mesh structure as a SOA so that we can apply more efficient copies and transforms like
            // http://fastcpp.blogspot.com/2011/07/fast-3d-vector-matrix-transformation.html
            dst->p = Transform( mesh.mTransform, src.p );
            //v4 pSrc = V4( src.p, 1 );
            //__m128 xyz1 = _mm_loadu_ps( (r32*)&pSrc );
            //__m128 x___ = _mm_dp_ps( r0, xyz1, 0xF1 );
            //__m128 _y__ = _mm_dp_ps( r1, xyz1, 0xF2 );
            //__m128 __z_ = _mm_dp_ps( r2, xyz1, 0xF4 );
            //__m128 xy__ = _mm_add_ps( x___, _y__ );
            //__m128 xyz_ = _mm_add_ps( xy__, __z_ );
            //_mm_storeu_ps( (r32*)&dst->p, xyz_ );

            dst->color = src.color;
            dst->uv = src.uv;

            commands->vertexBuffer.count++;
        }
        int indexOffsetStart = entry->vertexCount;
        entry->vertexCount += mesh.vertexCount;

        ASSERT( commands->indexBuffer.count + mesh.indexCount <= commands->indexBuffer.maxCount );

        for( u32 i = 0; i < mesh.indexCount; ++i )
        {
            u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
            *index = indexOffsetStart + mesh.indices[i];

            commands->indexBuffer.count++;
        }
        entry->indexCount += mesh.indexCount;
#endif
    }
}

void
RenderBounds( const aabb& box, u32 color, RenderCommands* renderCommands )
{
    RenderLine( V3( box.min.x, box.min.y, box.min.z ), V3( box.max.x, box.min.y, box.min.z ), color, renderCommands );
    RenderLine( V3( box.min.x, box.max.y, box.min.z ), V3( box.max.x, box.max.y, box.min.z ), color, renderCommands );
    RenderLine( V3( box.min.x, box.min.y, box.min.z ), V3( box.min.x, box.max.y, box.min.z ), color, renderCommands );
    RenderLine( V3( box.max.x, box.min.y, box.min.z ), V3( box.max.x, box.max.y, box.min.z ), color, renderCommands );

    RenderLine( V3( box.min.x, box.min.y, box.min.z ), V3( box.min.x, box.min.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.max.x, box.min.y, box.min.z ), V3( box.max.x, box.min.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.min.x, box.max.y, box.min.z ), V3( box.min.x, box.max.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.max.x, box.max.y, box.min.z ), V3( box.max.x, box.max.y, box.max.z ), color, renderCommands );

    RenderLine( V3( box.min.x, box.min.y, box.max.z ), V3( box.max.x, box.min.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.min.x, box.max.y, box.max.z ), V3( box.max.x, box.max.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.min.x, box.min.y, box.max.z ), V3( box.min.x, box.max.y, box.max.z ), color, renderCommands );
    RenderLine( V3( box.max.x, box.min.y, box.max.z ), V3( box.max.x, box.max.y, box.max.z ), color, renderCommands );
}

void
RenderBoundsAt( const v3& p, r32 size, u32 color, RenderCommands* renderCommands )
{
    aabb bounds = AABB( p, size );
    RenderBounds( bounds, color, renderCommands );
}

void
RenderBoxAt( const v3& p, r32 size, u32 color, RenderCommands* renderCommands )
{
    float halfSize = size * 0.5f;
    v3 p1, p2, p3, p4, p5, p6, p7, p8;

    p1 = p + V3( -1, -1,  1  ) * halfSize;
    p2 = p + V3(  1, -1,  1  ) * halfSize;
    p3 = p + V3(  1,  1,  1  ) * halfSize;
    p4 = p + V3( -1,  1,  1  ) * halfSize;
    p5 = p + V3( -1, -1, -1  ) * halfSize;
    p6 = p + V3(  1, -1, -1  ) * halfSize;
    p7 = p + V3(  1,  1, -1  ) * halfSize;
    p8 = p + V3( -1,  1, -1  ) * halfSize;

    // Up
    RenderQuad( p1, p2, p3, p4, color, renderCommands );

    // Down
    RenderQuad( p8, p7, p6, p5, color, renderCommands );

    // Front
    RenderQuad( p8, p4, p3, p7, color, renderCommands );

    // Back
    RenderQuad( p5, p6, p2, p1, color, renderCommands );

    // Left
    RenderQuad( p5, p1, p4, p8, color, renderCommands );

    // Right
    RenderQuad( p7, p3, p2, p6, color, renderCommands );
}

void
RenderFloorGrid( r32 areaSizeMeters, r32 resolutionMeters, RenderCommands* renderCommands )
{
    const r32 areaHalf = areaSizeMeters / 2;

    u32 semiBlack = Pack01ToRGBA( 0, 0, 0, 0.1f );
    v3 off = V3Zero;

    r32 yStart = -areaHalf;
    r32 yEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        RenderLine( V3( x, yStart, 0 ) + off, V3( x, yEnd, 0 ) + off, semiBlack, renderCommands );
    }
    r32 xStart = -areaHalf;
    r32 xEnd = areaHalf;
    for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
    {
        RenderLine( V3( xStart, y, 0 ) + off, V3( xEnd, y, 0 ) + off, semiBlack, renderCommands );
    }
}

void
RenderCubicGrid( const aabb& boundingBox, r32 step, u32 color, bool drawZAxis, RenderCommands* renderCommands )
{
    ASSERT( step > 0.f );

    v3 const& min = boundingBox.min;
    v3 const& max = boundingBox.max;

    for( r32 z = min.z; z <= max.z; z += step )
    {
        for( r32 y = min.y; y <= max.y; y += step )
            RenderLine( { min.x, y, z }, { max.x, y, z }, color, renderCommands );

        for( r32 x = min.x; x <= max.x; x += step )
            RenderLine( { x, min.y, z }, { x, max.y, z }, color, renderCommands );
    }

    if( drawZAxis )
    {
        for( r32 y = min.y; y <= max.y; y += step )
        {
            for( r32 x = min.x; x <= max.x; x += step )
                RenderLine( { x, y, min.z }, { x, y, max.z }, color, renderCommands );
        }
    }
}

inline void PushInstanceData( InstanceData const& data, RenderCommands* renderCommands )
{
    ASSERT( renderCommands->instanceBuffer.size + sizeof(InstanceData) <= renderCommands->instanceBuffer.maxSize );

    InstanceData* dst = (InstanceData*)(renderCommands->instanceBuffer.base + renderCommands->instanceBuffer.size);
    *dst = data;

    renderCommands->instanceBuffer.size += sizeof(InstanceData); 
}

void
RenderVoxelGrid( ClusterVoxelGrid const& voxelGrid, v3 const& clusterOffsetP, u32 color, RenderCommands* renderCommands )
{
    TIMED_BLOCK;

    RenderSetShader( ShaderProgramName::PlainColorVoxel, renderCommands );

    RenderEntryVoxelGrid* entry = PUSH_RENDER_ELEMENT( renderCommands, RenderEntryVoxelGrid );
    if( entry )
    {
        entry->vertexBufferOffset = renderCommands->vertexBuffer.count;
        entry->instanceBufferOffset = renderCommands->instanceBuffer.size;

        // Common instance data
        aabb box = AABB( V3Zero, VoxelSizeMeters );

        // TODO Only send the lines actually visible from the current lookAt vector of the camera (as a strip if possible!)
#define PUSH_LINE( p1, p2 )                                \
        PushVertex( p1, color, { 0, 0 }, renderCommands ); \
        PushVertex( p2, color, { 0, 0 }, renderCommands ); \

        PUSH_LINE( V3( box.min.x, box.min.y, box.min.z ), V3( box.max.x, box.min.y, box.min.z ) );
        PUSH_LINE( V3( box.min.x, box.max.y, box.min.z ), V3( box.max.x, box.max.y, box.min.z ) );
        PUSH_LINE( V3( box.min.x, box.min.y, box.min.z ), V3( box.min.x, box.max.y, box.min.z ) );
        PUSH_LINE( V3( box.max.x, box.min.y, box.min.z ), V3( box.max.x, box.max.y, box.min.z ) );

        PUSH_LINE( V3( box.min.x, box.min.y, box.min.z ), V3( box.min.x, box.min.y, box.max.z ) );
        PUSH_LINE( V3( box.max.x, box.min.y, box.min.z ), V3( box.max.x, box.min.y, box.max.z ) );
        PUSH_LINE( V3( box.min.x, box.max.y, box.min.z ), V3( box.min.x, box.max.y, box.max.z ) );
        PUSH_LINE( V3( box.max.x, box.max.y, box.min.z ), V3( box.max.x, box.max.y, box.max.z ) );

        PUSH_LINE( V3( box.min.x, box.min.y, box.max.z ), V3( box.max.x, box.min.y, box.max.z ) );
        PUSH_LINE( V3( box.min.x, box.max.y, box.max.z ), V3( box.max.x, box.max.y, box.max.z ) );
        PUSH_LINE( V3( box.min.x, box.min.y, box.max.z ), V3( box.min.x, box.max.y, box.max.z ) );
        PUSH_LINE( V3( box.max.x, box.min.y, box.max.z ), V3( box.max.x, box.max.y, box.max.z ) );
#undef PUSH_LINE


        // Per instance data
        InstanceData data = {};
        u32 instanceCount = 0;
        u8 const* grid = voxelGrid.data;

        for( int k = 0; k < VoxelsPerClusterAxis; ++k )
            for( int j = 0; j < VoxelsPerClusterAxis; ++j )
                for( int i = 0; i < VoxelsPerClusterAxis; ++i )
                {
                    //u8 voxelData = voxelGrid( i, j, k );
                    u8 voxelData = *grid++;
                    if( voxelData > 1 )
                    {
                        data.worldOffset = clusterOffsetP + V3( (r32)i, (r32)j, (r32)k ) * VoxelSizeMeters;
                        data.color = voxelData == 2 ? Pack01ToRGBA( 1, 0, 1, 1 ) : Pack01ToRGBA( 0, 0, 1, 1 );
                        PushInstanceData( data, renderCommands );

                        instanceCount++;
                    }
                }

        entry->instanceCount = instanceCount;
    }

    // https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
    // https://stackoverflow.com/questions/22948068/opengl-rendering-lots-of-cubes
    // http://jojendersie.de/rendering-huge-amounts-of-voxels/
}

void RenderClusterVoxels( Cluster const& cluster, v3 const& clusterOffsetP, u32 color, RenderCommands* renderCommands )
{
    TIMED_BLOCK;

    RenderSetShader( ShaderProgramName::PlainColorVoxel, renderCommands );

    RenderEntryVoxelChunk* entry = PUSH_RENDER_ELEMENT( renderCommands, RenderEntryVoxelChunk );
    if( entry )
    {
        // Common instance data
        //aabb box = AABB( V3Zero, VoxelSizeMeters );
        const v3 p0 = V3( 0, 0, 1 ) * VoxelSizeMeters;
        const v3 p1 = V3( 1, 0, 1 ) * VoxelSizeMeters;
        const v3 p2 = V3( 1, 1, 1 ) * VoxelSizeMeters;
        const v3 p3 = V3( 0, 1, 1 ) * VoxelSizeMeters;
        const v3 p4 = V3( 0, 0, 0 ) * VoxelSizeMeters;
        const v3 p5 = V3( 1, 0, 0 ) * VoxelSizeMeters;
        const v3 p6 = V3( 1, 1, 0 ) * VoxelSizeMeters;
        const v3 p7 = V3( 0, 1, 0 ) * VoxelSizeMeters;

        entry->vertexBufferOffset = renderCommands->vertexBuffer.count;
        entry->indexBufferOffset = renderCommands->indexBuffer.count;
        entry->instanceBufferOffset = renderCommands->instanceBuffer.size;

        PushVertex( p0, color, { 0, 0 }, renderCommands );
        PushVertex( p1, color, { 0, 0 }, renderCommands );
        PushVertex( p2, color, { 0, 0 }, renderCommands );
        PushVertex( p3, color, { 0, 0 }, renderCommands );
        PushVertex( p4, color, { 0, 0 }, renderCommands );
        PushVertex( p5, color, { 0, 0 }, renderCommands );
        PushVertex( p6, color, { 0, 0 }, renderCommands );
        PushVertex( p7, color, { 0, 0 }, renderCommands );

        // Up
        //RenderQuad( p0, p1, p2, p3, color, renderCommands );
        PushIndex( 0, renderCommands );
        PushIndex( 1, renderCommands );
        PushIndex( 2, renderCommands );
        PushIndex( 2, renderCommands );
        PushIndex( 3, renderCommands );
        PushIndex( 0, renderCommands );

        // Down
        //RenderQuad( p7, p6, p5, p4, color, renderCommands );
        PushIndex( 7, renderCommands );
        PushIndex( 6, renderCommands );
        PushIndex( 5, renderCommands );
        PushIndex( 5, renderCommands );
        PushIndex( 4, renderCommands );
        PushIndex( 7, renderCommands );

        // Front
        //RenderQuad( p7, p3, p2, p6, color, renderCommands );
        PushIndex( 7, renderCommands );
        PushIndex( 3, renderCommands );
        PushIndex( 2, renderCommands );
        PushIndex( 2, renderCommands );
        PushIndex( 6, renderCommands );
        PushIndex( 7, renderCommands );

        // Back
        //RenderQuad( p4, p5, p1, p0, color, renderCommands );
        PushIndex( 4, renderCommands );
        PushIndex( 5, renderCommands );
        PushIndex( 1, renderCommands );
        PushIndex( 1, renderCommands );
        PushIndex( 0, renderCommands );
        PushIndex( 4, renderCommands );

        // Left
        //RenderQuad( p4, p0, p3, p7, color, renderCommands );
        PushIndex( 4, renderCommands );
        PushIndex( 0, renderCommands );
        PushIndex( 3, renderCommands );
        PushIndex( 3, renderCommands );
        PushIndex( 7, renderCommands );
        PushIndex( 4, renderCommands );

        // Right
        //RenderQuad( p6, p2, p1, p5, color, renderCommands );
        PushIndex( 6, renderCommands );
        PushIndex( 2, renderCommands );
        PushIndex( 1, renderCommands );
        PushIndex( 1, renderCommands );
        PushIndex( 5, renderCommands );
        PushIndex( 6, renderCommands );

        // Per instance data
        InstanceData data = {};
        u32 instanceCount = 0;
#if 0
        u8 const* grid = cluster.voxelGrid.data;

        for( int k = 0; k < VoxelsPerClusterAxis; ++k )
            for( int j = 0; j < VoxelsPerClusterAxis; ++j )
                for( int i = 0; i < VoxelsPerClusterAxis; ++i )
                {
                    //u8 voxelData = cluster.voxelGrid( i, j, k );
                    u8 voxelData = *grid++;
                    if( voxelData > 1 )
                    {
                        data.worldOffset = clusterOffsetP + V3( (r32)i, (r32)j, (r32)k ) * VoxelSizeMeters;
                        data.color = voxelData == 2 ? Pack01ToRGBA( 0.99f, 0.9f, 0.7f, 1 ) : Pack01ToRGBA( 0, 0, 1, 1 );
                        PushInstanceData( data, renderCommands );

                        instanceCount++;
                    }
                }
#else
        for( u32 r = 0; r < cluster.rooms.count; ++r )
        {
            v3u roomMinP = cluster.rooms[r].voxelP;
            v3u roomMaxP = roomMinP + cluster.rooms[r].sizeVoxels - V3uOne;

            for( u32 k = roomMinP.z; k <= roomMaxP.z; ++k )
                for( u32 j = roomMinP.y; j <= roomMaxP.y; ++j )
                    for( u32 i = roomMinP.x; i <= roomMaxP.x; ++i )
                    {
                        bool atBorder = (i == roomMinP.x || i == roomMaxP.x)
                            || (j == roomMinP.y || j == roomMaxP.y)
                            || (k == roomMinP.z || k == roomMaxP.z);
                        if( atBorder )
                        {
                            data.worldOffset = clusterOffsetP + V3( (r32)i, (r32)j, (r32)k ) * VoxelSizeMeters;
                            data.color = Pack01ToRGBA( 0.99f, 0.9f, 0.7f, 1 );
                            PushInstanceData( data, renderCommands );

                            instanceCount++;
                        }
                    }
        }

        entry->instanceCount = instanceCount;
#endif
    // https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
    // https://stackoverflow.com/questions/22948068/opengl-rendering-lots-of-cubes
    // http://jojendersie.de/rendering-huge-amounts-of-voxels/
    }
}
