#ifndef __RENDERER_H__
#define __RENDERER_H__ 

enum class RenderEntryType
{
    RenderEntryClear,
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

#endif /* __RENDERER_H__ */
