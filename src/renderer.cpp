
#define PUSH_RENDER_ELEMENT(commands, type) (type *)_PushRenderElement( commands, sizeof(type), RenderEntryType::type )
internal RenderEntry *
_PushRenderElement( GameRenderCommands &commands, u32 size, RenderEntryType type )
{
    RenderEntry *result = 0;
    RenderBuffer &buffer = commands.renderBuffer;

    if( buffer.size + size < buffer.maxSize )
    {
        result = (RenderEntry *)(buffer.base + buffer.size);
        memset( result, 0, size );
        result->type = type;
        buffer.size += size;
    }
    else
    {
        // We don't just do an assert so we're fail tolerant in release builds
        INVALID_CODE_PATH
    }

    return result;
}

internal void
PushClear( GameRenderCommands &commands, v4 color )
{
    RenderEntryClear *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryClear );
    if( entry )
    {
        entry->color = color;
    }
}

internal void
PushRenderGroup( GameRenderCommands &commands, FlyingDude &dude )
{
    RenderEntryGroup *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryGroup );
    if( entry )
    {
        entry->vertices = dude.vertices;
        entry->vertexCount = ARRAYCOUNT( dude.vertices );
        entry->indices = dude.indices;
        entry->indexCount = ARRAYCOUNT( dude.indices );
        entry->mTransform = &dude.mTransform;
    }
}

internal void
PushRenderGroup( GameRenderCommands &commands, CubeThing &cube )
{
    RenderEntryGroup *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryGroup );
    if( entry )
    {
        entry->vertices = cube.vertices;
        entry->vertexCount = ARRAYCOUNT( cube.vertices );
        entry->indices = cube.indices;
        entry->indexCount = ARRAYCOUNT( cube.indices );
        entry->mTransform = &cube.mTransform;
    }
}
