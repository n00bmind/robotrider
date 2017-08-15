#ifndef __RENDERER_H__
#define __RENDERER_H__ 

enum class RenderEntryType
{
    RenderEntryClear,
    RenderEntryTexturedTri,
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

struct RenderEntryTexturedTri
{
    u32 vertexArrayOffset;
    u32 indexArrayOffset;
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
    u32 size;
    u32 maxSize;
};

struct IndexBuffer
{
    u32 *base;
    u32 size;
    u32 maxSize;
};

#endif /* __RENDERER_H__ */
