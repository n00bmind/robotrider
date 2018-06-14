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
    // OpenGLES,
    // Software?
};

enum class ShaderProgramName
{
    None,
    PlainColor,
    FlatShaded,
};


struct Camera
{
    r32 fovYDeg = 60;
    m4 mTransform = M4Identity();
};

struct TexturedVertex
{
    v3 p;
    u32 color;
    v2 uv;
    // TODO Should we just ignore these and do it all in the GS based on what shading we want?
    v3 n;
};


struct Material
{
    void* diffuseMap;
};

struct MarchingMeshPool;

struct Mesh
{
    TexturedVertex* vertices;
    u32 vertexCount;
    u32* indices;
    u32 indexCount;

    m4 mTransform;

    Material* material;

    MarchingMeshPool* ownerPool;
};


enum class RenderEntryType
{
    RenderEntryClear,
    RenderEntryTexturedTris,
    RenderEntryLines,
    RenderEntryProgramChange,
    RenderEntryMaterial,
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



struct RenderCommands
{
    u16 width;
    u16 height;

    Camera camera;

    RenderBuffer renderBuffer;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;

    RenderEntryTexturedTris *currentTris;
    RenderEntryLines *currentLines;
};

inline RenderCommands
InitRenderCommands( u8 *renderBuffer, u32 renderBufferMaxSize,
                    TexturedVertex *vertexBuffer, u32 vertexBufferMaxCount,
                    u32 *indexBuffer, u32 indexBufferMaxCount )
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

    result.currentTris = nullptr;
    result.currentLines = nullptr;

    return result;
}

inline void
ResetRenderCommands( RenderCommands *commands )
{
    commands->renderBuffer.size = 0;
    commands->vertexBuffer.count = 0;
    commands->indexBuffer.count = 0;
    commands->currentTris = 0;
}



inline u32
Pack01ToRGBA( r32 r, r32 g, r32 b, r32 a )
{
    u32 result = (((Round( a * 255 ) & 0xFF) << 24)
                | ((Round( b * 255 ) & 0xFF) << 16)
                | ((Round( g * 255 ) & 0xFF) << 8)
                | ((Round( r * 255 ) & 0xFF) << 0));
    return result;
}

inline u32
Pack01ToRGBA( v4 unpacked )
{
    u32 result = (((Round( unpacked.a * 255 ) & 0xFF) << 24)
                | ((Round( unpacked.b * 255 ) & 0xFF) << 16)
                | ((Round( unpacked.g * 255 ) & 0xFF) << 8)
                | ((Round( unpacked.r * 255 ) & 0xFF) << 0));
    return result;
}

#endif /* __RENDERER_H__ */
