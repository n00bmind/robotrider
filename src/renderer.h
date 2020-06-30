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

#ifndef __RENDERER_H__
#define __RENDERER_H__ 

#if NON_UNITY_BUILD
#include "common.h"
#include "math_types.h"
#include "data_types.h"
#endif


enum class Renderer
{
    OpenGL,
    Gnmx,
};

// TODO Turn into a StructEnum
enum class ShaderProgramName
{
    None,
    PlainColor,
    PlainColorVoxel,
    FlatShading,
};

enum class RenderSwitchType
{
    Culling,
};


struct Camera
{
    m4 worldToCamera;
    f32 fovYDeg;
};

inline Camera
DefaultCamera( f32 fovYDeg = 60 )
{
    Camera result = { M4Identity, fovYDeg };
    return result;
}

// TODO Test different layouts and investigate alignment etc here
struct TexturedVertex
{
    v3 p;
    u32 color;
    // TODO Should we just ignore these and do it all in the GS based on what shading we want?
    // TODO In any case, create a separate vertex format for all the debug stuff etc. that will never need this
    v3 n;
    v2 uv;
};

struct Texture
{
    void* handle;
    void* imageBuffer;

    i32 width;
    i32 height;
    i32 channelCount;
};

struct Material
{
    void* diffuseMap;
};

struct MeshPool;

struct Mesh
{
    MeshPool* ownerPool;

    Array<TexturedVertex> vertices;
    Array<i32> indices;

    Material* material;
    aabb bounds;

    // NOTE Static geometry doesnt need this at all
    // NOTE Dynamic geometry will be always transformed relative to the cluster they live in
    m4 mTransform;

    // Index into the global cluster offset array to determine final translation in the shader
    i32 simClusterIndex;
};

inline void
InitMesh( Mesh* mesh )
{
    *mesh = {};
    mesh->mTransform = M4Identity;
    mesh->simClusterIndex = 0;
}

inline bool
Empty( Mesh const& mesh )
{
    return mesh.vertices.count == 0;
}

inline Mesh
CreateMeshFromBuffers( BucketArray<TexturedVertex> const& vertices, BucketArray<i32> const& indices, MemoryArena* arena )
{
    Mesh result;
    InitMesh( &result );

    INIT( &result.vertices ) Array<TexturedVertex>( arena, vertices.count );
    vertices.CopyTo( &result.vertices );
    INIT( &result.indices ) Array<i32>( arena, indices.count );
    indices.CopyTo( &result.indices );

    return result;
}

inline void
CalcBounds( Mesh* mesh )
{
    aabb result = { V3Inf, -V3Inf };

    for( int i = 0; i < mesh->vertices.count; ++i )
    {
        v3& p = mesh->vertices[i].p;

        if( result.min.x > p.x )
            result.min.x = p.x;
        if( result.max.x < p.x )
            result.max.x = p.x;

        if( result.min.y > p.y )
            result.min.y = p.y;
        if( result.max.y < p.y )
            result.max.y = p.y;

        if( result.min.z > p.z )
            result.min.z = p.z;
        if( result.max.z < p.z )
            result.max.z = p.z;
    }

    mesh->bounds = result;
}


struct InstanceData
{
    v3 worldOffset;
    u32 color;
};

struct MeshData
{
    i32 vertexCount;
    i32 indexCount;
    i32 indexStartOffset;
    i32 simClusterIndex;
};


enum class RenderEntryType
{
    RenderEntryClear,
    RenderEntryTexturedTris,
    RenderEntryLines,
    RenderEntryProgramChange,
    RenderEntryMaterial,
    RenderEntrySwitch,
    RenderEntryVoxelGrid,
    RenderEntryVoxelChunk,
    RenderEntryMeshChunk,
};

struct RenderEntry
{
    RenderEntryType type;
    i32 size;
};

struct RenderEntryClear
{
    RenderEntry header;

    v4 color;
};

struct RenderEntryTexturedTris
{
    RenderEntry header;

    i32 vertexBufferOffset;
    i32 vertexCount;
    i32 indexBufferOffset;
    i32 indexCount;

    // Material **materialArray;
};

struct RenderEntryLines
{
    RenderEntry header;

    i32 vertexBufferOffset;
    i32 lineCount;
};

struct RenderEntryProgramChange
{
    RenderEntry header;

    ShaderProgramName programName;
};

struct RenderEntryMaterial
{
    RenderEntry header;

    Material* material;
};

struct RenderEntrySwitch
{
    RenderEntry header;

    RenderSwitchType renderSwitch;
    bool enable;
};

struct RenderEntryVoxelGrid
{
    RenderEntry header;

    i32 vertexBufferOffset;
    i32 instanceBufferOffset;
    i32 instanceCount;
};

struct RenderEntryVoxelChunk
{
    RenderEntry header;

    i32 vertexBufferOffset;
    i32 indexBufferOffset;
    i32 instanceBufferOffset;
    i32 instanceCount;
};

struct RenderEntryMeshChunk
{
    RenderEntry header;

    i32 vertexBufferOffset;
    i32 indexBufferOffset;
    i32 instanceBufferOffset;
    i32 meshCount;

    i32 runningVertexCount;
};


struct RenderBuffer
{
    u8 *base;
    i32 size;
    i32 maxSize;
};

struct VertexBuffer
{
    TexturedVertex *base;
    i32 count;
    i32 maxCount;
};

struct IndexBuffer
{
    i32 *base;
    i32 count;
    i32 maxCount;
};

struct InstanceBuffer
{
    u8* base;
    i32 size;
    i32 maxSize;
};



struct RenderCommands
{
    RenderBuffer   renderBuffer;
    VertexBuffer   vertexBuffer;
    IndexBuffer    indexBuffer;
    InstanceBuffer instanceBuffer;

    RenderEntryTexturedTris *currentTris;
    RenderEntryLines *currentLines;
    RenderEntryMeshChunk* currentMeshChunk;

    Camera camera;

    u16 width;
    u16 height;

    v3* simClusterOffsets;
    i32 simClusterCount;

    bool isValid;
};

inline RenderCommands
InitRenderCommands( u8 *renderBuffer, int renderBufferMaxSize,
                    TexturedVertex *vertexBuffer, int vertexBufferMaxCount,
                    i32 *indexBuffer, int indexBufferMaxCount,
                    u8* instanceBuffer, int instanceBufferMaxSize )
{
    ASSERT( renderBufferMaxSize > 0 && vertexBufferMaxCount > 0 && indexBufferMaxCount > 0 && instanceBufferMaxSize > 0 );

    RenderCommands result;

    result.renderBuffer.base = renderBuffer;
    result.renderBuffer.size = 0;
    result.renderBuffer.maxSize = renderBufferMaxSize;
    result.vertexBuffer.base = vertexBuffer;
    result.vertexBuffer.count = 0;
    result.vertexBuffer.maxCount = vertexBufferMaxCount;
    result.indexBuffer.base = indexBuffer;
    result.indexBuffer.count = 0;
    result.indexBuffer.maxCount = indexBufferMaxCount;
    result.instanceBuffer.base = instanceBuffer;
    result.instanceBuffer.size = 0;
    result.instanceBuffer.maxSize = instanceBufferMaxSize;

    result.currentTris = nullptr;
    result.currentLines = nullptr;
    result.currentMeshChunk = nullptr;

    result.isValid = renderBuffer && vertexBuffer && indexBuffer && instanceBuffer;

    return result;
}

inline void
ResetRenderCommands( RenderCommands *commands )
{
    commands->renderBuffer.size = 0;
    commands->vertexBuffer.count = 0;
    commands->indexBuffer.count = 0;
    commands->instanceBuffer.size = 0;

    commands->currentTris = nullptr;
    commands->currentLines = nullptr;
    commands->currentMeshChunk = nullptr;
}


struct Cluster;
typedef Grid3D<u8> ClusterVoxelGrid;

void RenderClear( const v4& color, RenderCommands *commands );
void RenderQuad( const v3 &p1, const v3 &p2, const v3 &p3, const v3 &p4, u32 color, RenderCommands *commands );
void RenderLine( v3 pStart, v3 pEnd, u32 color, RenderCommands *commands );
void RenderSetShader( ShaderProgramName programName, RenderCommands *commands );
void RenderSetMaterial( Material* material, RenderCommands* commands );
void RenderSwitch( RenderSwitchType renderSwitch, bool enable, RenderCommands* commands );
void RenderMesh( const Mesh& mesh, RenderCommands *commands );
void RenderBounds( const aabb& box, u32 color, RenderCommands* renderCommands );
void RenderBoundsAt( const v3& p, f32 size, u32 color, RenderCommands* renderCommands );
void RenderBoxAt( const v3& p, f32 size, u32 color, RenderCommands* renderCommands );
void RenderFloorGrid( f32 areaSizeMeters, f32 resolutionMeters, RenderCommands* renderCommands );
void RenderCubicGrid( const aabb& boundingBox, f32 step, u32 color, bool drawZAxis, RenderCommands* renderCommands );
void RenderVoxelGrid( ClusterVoxelGrid const& voxelGrid, v3 const& clusterOffsetP, u32 color, RenderCommands* renderCommands );
void RenderClusterVoxels( Cluster const& cluster, v3 const& clusterOffsetP, u32 color, RenderCommands* renderCommands );




#endif /* __RENDERER_H__ */
