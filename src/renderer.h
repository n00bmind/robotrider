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
    RenderEntryGroup,
};

struct RenderEntry
{
    RenderEntryType type;
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
};

struct RenderEntryTexturedTris
{
    RenderEntry header;

    u32 vertexBufferOffset;
    u32 indexBufferOffset;
    // Material **materialArray;
    u32 triCount;
};

struct RenderEntryGroup
{
    RenderEntry header;

    v3 *vertices;
    u32 vertexCount;
    u32 *indices;
    u32 indexCount;

    m4 *mTransform;
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
