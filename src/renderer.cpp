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
#include <string.h>
#include "debugstats.h"
#include "world.h"
//#include "renderer.h"
#endif


#define PUSH_RENDER_ELEMENT(commands, type) (type *)_PushRenderElement( commands, sizeof(type), RenderEntryType::type )
internal RenderEntry *
_PushRenderElement( RenderCommands *commands, int size, RenderEntryType type )
{
    RenderEntry *result = 0;
    RenderBuffer &buffer = commands->renderBuffer;

    if( buffer.size + size < buffer.maxSize )
    {
        result = (RenderEntry *)(buffer.base + buffer.size);
        PZERO( result, Sz( size ) );
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
    commands->currentMeshChunk = nullptr;

    return result;
}

void RenderClear( const v4& color, RenderCommands *commands )
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

internal RenderEntryMeshChunk*
GetOrCreateCurrentMeshChunk( RenderCommands* commands )
{
    if( !commands->currentMeshChunk )
    {
        RenderEntryMeshChunk* chunk = PUSH_RENDER_ELEMENT( commands, RenderEntryMeshChunk );
        chunk->vertexBufferOffset = commands->vertexBuffer.count;
        chunk->indexBufferOffset = commands->indexBuffer.count;
        chunk->instanceBufferOffset = commands->instanceBuffer.size;
        chunk->meshCount = 0;
        chunk->runningVertexCount = 0;

        commands->currentMeshChunk = chunk;
    }

    return commands->currentMeshChunk;
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
PushVertices( TexturedVertex const* vertexBase, int vertexCount, RenderCommands* commands )
{
    ASSERT( commands->vertexBuffer.count + vertexCount <= commands->vertexBuffer.maxCount );

    PCOPY( vertexBase, commands->vertexBuffer.base + commands->vertexBuffer.count, vertexCount * sizeof(TexturedVertex) );
    commands->vertexBuffer.count += vertexCount;
}

inline internal void
PushIndex( i32 value, RenderCommands* commands )
{
    //TIMED_BLOCK;

    ASSERT( commands->indexBuffer.count + 1 <= commands->indexBuffer.maxCount );

    i32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
    *index = value;

    commands->indexBuffer.count++;
}

inline internal void
PushIndices( i32 const* indexBase, int indexCount, RenderCommands* commands )
{
    ASSERT( commands->indexBuffer.count + indexCount <= commands->indexBuffer.maxCount );

    PCOPY( indexBase, commands->indexBuffer.base + commands->indexBuffer.count, indexCount * sizeof(i32) );
    commands->indexBuffer.count += indexCount;
}

inline internal void
PushInstanceData( InstanceData const& data, RenderCommands* commands )
{
    ASSERT( commands->instanceBuffer.size + SIZE(InstanceData) <= commands->instanceBuffer.maxSize );

    InstanceData* dst = (InstanceData*)(commands->instanceBuffer.base + commands->instanceBuffer.size);
    *dst = data;

    commands->instanceBuffer.size += sizeof(InstanceData); 
}

inline internal void
PushMeshData( int vertexCount, int indexCount, int indexStartOffset, int simClusterIndex, RenderCommands* commands )
{
    ASSERT( commands->instanceBuffer.size + SIZE(MeshData) <= commands->instanceBuffer.maxSize );

    MeshData* data = (MeshData*)(commands->instanceBuffer.base + commands->instanceBuffer.size);
    data->vertexCount = vertexCount;
    data->indexCount = indexCount;
    data->indexStartOffset = indexStartOffset;
    data->simClusterIndex = simClusterIndex;

    commands->instanceBuffer.size += sizeof(MeshData); 
}

// Push 4 vertices (1st vertex is "top-left" and counter-clockwise from there)
void RenderQuad( const v3 &p1, const v3 &p2, const v3 &p3, const v3 &p4, u32 color, RenderCommands *commands )
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

void RenderLine( v3 pStart, v3 pEnd, u32 color, RenderCommands *commands )
{
    RenderEntryLines *entry = GetOrCreateCurrentLines( commands );
    if( entry )
    {
        PushVertex( pStart, color, { 0, 0 }, commands );
        PushVertex( pEnd, color, { 0, 0 }, commands );

        ++entry->lineCount;
    }
}

void RenderSetShader( ShaderProgramName programName, RenderCommands *commands )
{
    RenderEntryProgramChange *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryProgramChange );
    if( entry )
    {
        entry->programName = programName;
    }
}

void RenderSetMaterial( Material* material, RenderCommands* commands )
{
    RenderEntryMaterial *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryMaterial );
    if( entry )
    {
        entry->material = material;
    }
}

void RenderSwitch( RenderSwitchType renderSwitch, bool enable, RenderCommands* commands )
{
    RenderEntrySwitch *entry = PUSH_RENDER_ELEMENT( commands, RenderEntrySwitch );
    if( entry )
    {
        entry->renderSwitch = renderSwitch;
        entry->enable = enable;
    }
}



void RenderMesh( const Mesh& mesh, RenderCommands *commands )
{
    TIMED_BLOCK;

#if 0
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        for( int i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& v = mesh.vertices[i];

            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( /*mesh.mTransform **/ v.p, v.color, v.uv, commands );
        }
        int indexOffsetStart = entry->vertexCount;
        entry->vertexCount += mesh.vertexCount;

        for( int i = 0; i < mesh.indexCount; ++i )
        {
            PushIndex( indexOffsetStart + mesh.indices[i], commands );
        }
        entry->indexCount += mesh.indexCount;
    }
#else
    RenderEntryMeshChunk* entry = GetOrCreateCurrentMeshChunk( commands );
    if( entry )
    {
        int indexStartOffset = entry->runningVertexCount;

#if 0
        PushVertices( mesh.vertices.data, mesh.vertices.count, commands );
#else
        for( int i = 0; i < mesh.vertices.count; ++i )
        {
            TexturedVertex const& v = mesh.vertices[i];
            // Transform to world coordinates so this can all be rendered in big chunks
            // FIXME We should be using a TBO and do the transform in the VS
            PushVertex( mesh.mTransform * v.p, v.color, v.uv, commands );
        }
#endif

        entry->runningVertexCount += mesh.vertices.count;
        PushIndices( mesh.indices.data, mesh.indices.count, commands );

        PushMeshData( mesh.vertices.count, mesh.indices.count, indexStartOffset, mesh.simClusterIndex, commands );
        entry->meshCount++;
    }
#endif
}

void RenderBounds( const aabb& box, u32 color, RenderCommands* commands )
{
    v3 min, max;
    MinMax( box, &min, &max );

    RenderLine( V3( min.x, min.y, min.z ), V3( max.x, min.y, min.z ), color, commands );
    RenderLine( V3( min.x, max.y, min.z ), V3( max.x, max.y, min.z ), color, commands );
    RenderLine( V3( min.x, min.y, min.z ), V3( min.x, max.y, min.z ), color, commands );
    RenderLine( V3( max.x, min.y, min.z ), V3( max.x, max.y, min.z ), color, commands );

    RenderLine( V3( min.x, min.y, min.z ), V3( min.x, min.y, max.z ), color, commands );
    RenderLine( V3( max.x, min.y, min.z ), V3( max.x, min.y, max.z ), color, commands );
    RenderLine( V3( min.x, max.y, min.z ), V3( min.x, max.y, max.z ), color, commands );
    RenderLine( V3( max.x, max.y, min.z ), V3( max.x, max.y, max.z ), color, commands );

    RenderLine( V3( min.x, min.y, max.z ), V3( max.x, min.y, max.z ), color, commands );
    RenderLine( V3( min.x, max.y, max.z ), V3( max.x, max.y, max.z ), color, commands );
    RenderLine( V3( min.x, min.y, max.z ), V3( min.x, max.y, max.z ), color, commands );
    RenderLine( V3( max.x, min.y, max.z ), V3( max.x, max.y, max.z ), color, commands );
}

void RenderBoundsAt( const v3& p, f32 size, u32 color, RenderCommands* commands )
{
    aabb bounds = AABBCenterSize( p, size );
    RenderBounds( bounds, color, commands );
}

void RenderBoxAt( const v3& p, f32 size, u32 color, RenderCommands* commands )
{
    float halfSize = size * 0.5f;
    v3 p1, p2, p3, p4, p5, p6, p7, p8;

    p1 = p + V3( -1.f, -1.f,  1.f  ) * halfSize;
    p2 = p + V3(  1.f, -1.f,  1.f  ) * halfSize;
    p3 = p + V3(  1.f,  1.f,  1.f  ) * halfSize;
    p4 = p + V3( -1.f,  1.f,  1.f  ) * halfSize;
    p5 = p + V3( -1.f, -1.f, -1.f  ) * halfSize;
    p6 = p + V3(  1.f, -1.f, -1.f  ) * halfSize;
    p7 = p + V3(  1.f,  1.f, -1.f  ) * halfSize;
    p8 = p + V3( -1.f,  1.f, -1.f  ) * halfSize;

    // Up
    RenderQuad( p1, p2, p3, p4, color, commands );

    // Down
    RenderQuad( p8, p7, p6, p5, color, commands );

    // Front
    RenderQuad( p8, p4, p3, p7, color, commands );

    // Back
    RenderQuad( p5, p6, p2, p1, color, commands );

    // Left
    RenderQuad( p5, p1, p4, p8, color, commands );

    // Right
    RenderQuad( p7, p3, p2, p6, color, commands );
}

void RenderFloorGrid( f32 areaSizeMeters, f32 resolutionMeters, RenderCommands* commands )
{
    const f32 areaHalf = areaSizeMeters / 2;

    u32 semiBlack = Pack01ToRGBA( 0, 0, 0, 0.1f );
    v3 off = V3Zero;

    f32 yStart = -areaHalf;
    f32 yEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        RenderLine( V3( x, yStart, 0.f ) + off, V3( x, yEnd, 0.f ) + off, semiBlack, commands );
    }
    f32 xStart = -areaHalf;
    f32 xEnd = areaHalf;
    for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
    {
        RenderLine( V3( xStart, y, 0.f ) + off, V3( xEnd, y, 0.f ) + off, semiBlack, commands );
    }
}

void RenderCubicGrid( const aabb& boundingBox, f32 step, u32 color, bool drawZAxis, RenderCommands* commands )
{
    ASSERT( step > 0.f );

    v3 min, max;
    MinMax( boundingBox, &min, &max );

    for( f32 z = min.z; z <= max.z; z += step )
    {
        for( f32 y = min.y; y <= max.y; y += step )
            RenderLine( { min.x, y, z }, { max.x, y, z }, color, commands );

        for( f32 x = min.x; x <= max.x; x += step )
            RenderLine( { x, min.y, z }, { x, max.y, z }, color, commands );
    }

    if( drawZAxis )
    {
        for( f32 y = min.y; y <= max.y; y += step )
        {
            for( f32 x = min.x; x <= max.x; x += step )
                RenderLine( { x, y, min.z }, { x, y, max.z }, color, commands );
        }
    }
}

void RenderVoxelGrid( ClusterVoxelGrid const& voxelGrid, v3 const& clusterOffsetP, u32 color, RenderCommands* commands )
{
    TIMED_BLOCK;

    RenderSetShader( ShaderProgramName::PlainColorVoxel, commands );

    RenderEntryVoxelGrid* entry = PUSH_RENDER_ELEMENT( commands, RenderEntryVoxelGrid );
    if( entry )
    {
        entry->vertexBufferOffset = commands->vertexBuffer.count;
        entry->instanceBufferOffset = commands->instanceBuffer.size;

        // Common instance data
        v3 min = V3Zero - V3( VoxelSizeMeters * 0.5f );
        v3 max = V3Zero + V3( VoxelSizeMeters * 0.5f );

        // TODO Only send the lines actually visible from the current lookAt vector of the camera (as a strip if possible!)
#define PUSH_LINE( p1, p2 )                                \
        PushVertex( p1, color, { 0, 0 }, commands ); \
        PushVertex( p2, color, { 0, 0 }, commands ); \

        PUSH_LINE( V3( min.x, min.y, min.z ), V3( max.x, min.y, min.z ) );
        PUSH_LINE( V3( min.x, max.y, min.z ), V3( max.x, max.y, min.z ) );
        PUSH_LINE( V3( min.x, min.y, min.z ), V3( min.x, max.y, min.z ) );
        PUSH_LINE( V3( max.x, min.y, min.z ), V3( max.x, max.y, min.z ) );

        PUSH_LINE( V3( min.x, min.y, min.z ), V3( min.x, min.y, max.z ) );
        PUSH_LINE( V3( max.x, min.y, min.z ), V3( max.x, min.y, max.z ) );
        PUSH_LINE( V3( min.x, max.y, min.z ), V3( min.x, max.y, max.z ) );
        PUSH_LINE( V3( max.x, max.y, min.z ), V3( max.x, max.y, max.z ) );

        PUSH_LINE( V3( min.x, min.y, max.z ), V3( max.x, min.y, max.z ) );
        PUSH_LINE( V3( min.x, max.y, max.z ), V3( max.x, max.y, max.z ) );
        PUSH_LINE( V3( min.x, min.y, max.z ), V3( min.x, max.y, max.z ) );
        PUSH_LINE( V3( max.x, min.y, max.z ), V3( max.x, max.y, max.z ) );
#undef PUSH_LINE


        // Per instance data
        InstanceData data = {};
        int instanceCount = 0;
        u8 const* grid = voxelGrid.data;

        for( int k = 0; k < VoxelsPerClusterAxis; ++k )
            for( int j = 0; j < VoxelsPerClusterAxis; ++j )
                for( int i = 0; i < VoxelsPerClusterAxis; ++i )
                {
                    //u8 voxelData = voxelGrid( i, j, k );
                    u8 voxelData = *grid++;
                    if( voxelData > 1 )
                    {
                        data.worldOffset = clusterOffsetP + V3( (f32)i, (f32)j, (f32)k ) * VoxelSizeMeters;
                        data.color = voxelData == 2 ? Pack01ToRGBA( 1, 0, 1, 1 ) : Pack01ToRGBA( 0, 0, 1, 1 );
                        PushInstanceData( data, commands );

                        instanceCount++;
                    }
                }

        entry->instanceCount = instanceCount;
    }

    // https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
    // https://stackoverflow.com/questions/22948068/opengl-rendering-lots-of-cubes
    // http://jojendersie.de/rendering-huge-amounts-of-voxels/
}

void RenderClusterVoxels( Cluster const& cluster, v3 const& clusterOffsetP, u32 color, RenderCommands* commands )
{
    TIMED_BLOCK;

    RenderSetShader( ShaderProgramName::PlainColorVoxel, commands );

    RenderEntryVoxelChunk* entry = PUSH_RENDER_ELEMENT( commands, RenderEntryVoxelChunk );
    if( entry )
    {
        // Common instance data
        //aabb box = AABB( V3Zero, VoxelSizeMeters );
        const v3 p0 = V3( 0.f, 0.f, 1.f ) * VoxelSizeMeters;
        const v3 p1 = V3( 1.f, 0.f, 1.f ) * VoxelSizeMeters;
        const v3 p2 = V3( 1.f, 1.f, 1.f ) * VoxelSizeMeters;
        const v3 p3 = V3( 0.f, 1.f, 1.f ) * VoxelSizeMeters;
        const v3 p4 = V3( 0.f, 0.f, 0.f ) * VoxelSizeMeters;
        const v3 p5 = V3( 1.f, 0.f, 0.f ) * VoxelSizeMeters;
        const v3 p6 = V3( 1.f, 1.f, 0.f ) * VoxelSizeMeters;
        const v3 p7 = V3( 0.f, 1.f, 0.f ) * VoxelSizeMeters;

        entry->vertexBufferOffset = commands->vertexBuffer.count;
        entry->indexBufferOffset = commands->indexBuffer.count;
        entry->instanceBufferOffset = commands->instanceBuffer.size;

        PushVertex( p0, color, { 0, 0 }, commands );
        PushVertex( p1, color, { 0, 0 }, commands );
        PushVertex( p2, color, { 0, 0 }, commands );
        PushVertex( p3, color, { 0, 0 }, commands );
        PushVertex( p4, color, { 0, 0 }, commands );
        PushVertex( p5, color, { 0, 0 }, commands );
        PushVertex( p6, color, { 0, 0 }, commands );
        PushVertex( p7, color, { 0, 0 }, commands );

        // Up
        //RenderQuad( p0, p1, p2, p3, color, commands );
        PushIndex( 0, commands );
        PushIndex( 1, commands );
        PushIndex( 2, commands );
        PushIndex( 2, commands );
        PushIndex( 3, commands );
        PushIndex( 0, commands );

        // Down
        //RenderQuad( p7, p6, p5, p4, color, commands );
        PushIndex( 7, commands );
        PushIndex( 6, commands );
        PushIndex( 5, commands );
        PushIndex( 5, commands );
        PushIndex( 4, commands );
        PushIndex( 7, commands );

        // Front
        //RenderQuad( p7, p3, p2, p6, color, commands );
        PushIndex( 7, commands );
        PushIndex( 3, commands );
        PushIndex( 2, commands );
        PushIndex( 2, commands );
        PushIndex( 6, commands );
        PushIndex( 7, commands );

        // Back
        //RenderQuad( p4, p5, p1, p0, color, commands );
        PushIndex( 4, commands );
        PushIndex( 5, commands );
        PushIndex( 1, commands );
        PushIndex( 1, commands );
        PushIndex( 0, commands );
        PushIndex( 4, commands );

        // Left
        //RenderQuad( p4, p0, p3, p7, color, commands );
        PushIndex( 4, commands );
        PushIndex( 0, commands );
        PushIndex( 3, commands );
        PushIndex( 3, commands );
        PushIndex( 7, commands );
        PushIndex( 4, commands );

        // Right
        //RenderQuad( p6, p2, p1, p5, color, commands );
        PushIndex( 6, commands );
        PushIndex( 2, commands );
        PushIndex( 1, commands );
        PushIndex( 1, commands );
        PushIndex( 5, commands );
        PushIndex( 6, commands );

        // Per instance data
        InstanceData data = {};
        int instanceCount = 0;
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
                        data.worldOffset = clusterOffsetP + V3( (f32)i, (f32)j, (f32)k ) * VoxelSizeMeters;
                        data.color = voxelData == 2 ? Pack01ToRGBA( 0.99f, 0.9f, 0.7f, 1 ) : Pack01ToRGBA( 0, 0, 1, 1 );
                        PushInstanceData( data, commands );

                        instanceCount++;
                    }
                }
#else
        for( int r = 0; r < cluster.rooms.count; ++r )
        {
            v3i roomMinP = cluster.rooms[r].voxelP;
            v3i roomMaxP = roomMinP + cluster.rooms[r].sizeVoxels - V3iOne;

            for( int k = roomMinP.z; k <= roomMaxP.z; ++k )
                for( int j = roomMinP.y; j <= roomMaxP.y; ++j )
                    for( int i = roomMinP.x; i <= roomMaxP.x; ++i )
                    {
                        bool atBorder = (i == roomMinP.x || i == roomMaxP.x)
                            || (j == roomMinP.y || j == roomMaxP.y)
                            || (k == roomMinP.z || k == roomMaxP.z);
                        if( atBorder )
                        {
                            data.worldOffset = clusterOffsetP + V3( (f32)i, (f32)j, (f32)k ) * VoxelSizeMeters;
                            data.color = Pack01ToRGBA( 0.99f, 0.9f, 0.7f, 1 );
                            PushInstanceData( data, commands );

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
