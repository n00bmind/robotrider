
internal RenderBuffer *
AllocateRenderBuffer( MemoryArena *arena, u32 maxSize )
{
    RenderBuffer *result = PUSH_STRUCT( arena, RenderBuffer );
    result->base = (u8 *)PUSH_SIZE( arena, maxSize );
    result->size = 0;
    result->maxSize = maxSize;

    return result;
}

internal void *
_PushRenderElement( GameRenderCommands &commands, u32 size )
{
    void *result = 0;
    RenderBuffer *buffer = commands.renderBuffer;

    if( buffer->size + size < buffer->maxSize )
    {
        result = buffer->base + buffer->size;
        buffer->size += size;
        memset( result, 0, size );
    }
    else
    {
        // We don't just do an assert so we're fail tolerant in release builds
        INVALID_CODE_PATH
    }

    return result;
}

internal void
PushRenderGroup( GameRenderCommands &commands, FlyingDude &dude, bool rebuildGeometry = false )
{
    RenderGroup *entry = (RenderGroup *)_PushRenderElement( commands, sizeof(RenderGroup) );

    entry->vertices = dude.vertices;
    entry->vertexCount = ARRAYCOUNT( dude.vertices );
    entry->indices = dude.indices;
    entry->indexCount = ARRAYCOUNT( dude.indices );
    entry->mTransform = &dude.mTransform;
    entry->renderHandle = &dude.renderHandle;
}

internal void
PushRenderGroup( GameRenderCommands &commands, CubeThing &cube, bool rebuildGeometry = false )
{
    RenderGroup *entry = (RenderGroup *)_PushRenderElement( commands, sizeof(RenderGroup) );

    entry->vertexCount = ARRAYCOUNT( cube.vertices );
    entry->indices = cube.indices;
    entry->indexCount = ARRAYCOUNT( cube.indices );
    entry->mTransform = &cube.mTransform;
    entry->renderHandle = &cube.renderHandle;
}
