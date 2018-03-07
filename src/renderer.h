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
    // TODO OpenGLES,
    // Software?
};


struct Camera
{
    r32 fovYDeg = 60;
    m4 mTransform = M4Identity();
};


enum class RenderEntryType
{
    RenderEntryClear,
    RenderEntryTexturedTris,
    RenderEntryLines,
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

struct TexturedVertex
{
    v3 p;
    u32 color;
    v2 uv;
    // TODO Should we just ignore these and do it all in the GS based on what shading we want?
    v3 normal;
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

#endif /* __RENDERER_H__ */
