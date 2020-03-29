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

enum class Renderer
{
    OpenGL,
    Gnmx,
    // OpenGLES,
    // Software?
};

enum class ShaderProgramName
{
    None,
    PlainColor,
    PlainColorVoxel,
    FlatShading,
};

enum class RenderSwitch
{
    Bla,
};


struct Camera
{
    r32 fovYDeg;
    m4 worldToCamera;
};

inline Camera
DefaultCamera( r32 fovYDeg = 60 )
{
    Camera result = { fovYDeg, M4Identity };
    return result;
}

struct TexturedVertex
{
    v3 p;
    u32 color;
    v2 uv;
    // TODO Should we just ignore these and do it all in the GS based on what shading we want?
    // TODO In any case, create a separate vertex format for all the debug stuff etc. that will never need this
    v3 n;
};

struct Texture
{
    void* handle;
    void* imageBuffer;

    u32 width;
    u32 height;
    u32 channelCount;
};

struct Material
{
    void* diffuseMap;
};

struct MeshPool;

struct Mesh
{
    TexturedVertex* vertices;
    u32 vertexCount;
    u32* indices;
    u32 indexCount;

    m4 mTransform;
    aabb bounds;

    Material* material;

    MeshPool* ownerPool;
};

inline void
Init( Mesh* mesh )
{
    *mesh = {};
    mesh->mTransform = M4Identity;
}

inline void
CalcBounds( Mesh* mesh )
{
    aabb result = { V3Inf, -V3Inf };

    for( u32 i = 0; i < mesh->vertexCount; ++i )
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


enum class RenderEntryType
{
    RenderEntryClear,
    RenderEntryTexturedTris,
    RenderEntryLines,
    RenderEntryProgramChange,
    RenderEntryMaterial,
    RenderEntrySwitch,
    RenderEntryVoxelGrid,
};

struct RenderEntry
{
    RenderEntryType type;
    u32 size;
};

struct RenderEntryClear
{
    RenderEntry header;

    v4 color;
};

struct RenderEntryTexturedTris
{
    RenderEntry header;

    u32 vertexBufferOffset;
    u32 vertexCount;
    u32 indexBufferOffset;
    u32 indexCount;

    // Material **materialArray;
};

struct RenderEntryLines
{
    RenderEntry header;

    u32 vertexBufferOffset;
    u32 lineCount;
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

    RenderSwitch renderSwitch;
    bool enable;
};

struct RenderEntryVoxelGrid
{
    RenderEntry header;

    u32 vertexBufferOffset;
    u32 instanceBufferOffset;
    u32 instanceCount;
};



struct RenderBuffer
{
    u8 *base;
    u32 size;
    u32 maxSize;
};

struct VertexBuffer
{
    TexturedVertex *base;
    u32 count;
    u32 maxCount;
};

struct IndexBuffer
{
    u32 *base;
    u32 count;
    u32 maxCount;
};

struct InstanceBuffer
{
    u8* base;
    u32 size;
    u32 maxSize;
};



struct RenderCommands
{
    u16 width;
    u16 height;

    Camera camera;

    RenderBuffer   renderBuffer;
    VertexBuffer   vertexBuffer;
    IndexBuffer    indexBuffer;
    InstanceBuffer instanceBuffer;

    RenderEntryTexturedTris *currentTris;
    RenderEntryLines *currentLines;

    bool isValid;
};

inline RenderCommands
InitRenderCommands( u8 *renderBuffer, u32 renderBufferMaxSize,
                    TexturedVertex *vertexBuffer, u32 vertexBufferMaxCount,
                    u32 *indexBuffer, u32 indexBufferMaxCount,
                    u8* instanceBuffer, u32 instanceBufferMaxSize )
{
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
}



#endif /* __RENDERER_H__ */
