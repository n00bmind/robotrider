
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

    // Reset currentTris so a new bundle is started
    commands.currentTris = 0;

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

internal RenderEntryTexturedTris *
GetOrCreateCurrentTris( GameRenderCommands &commands )
{
    // FIXME Artificially break the current bundle when we get to an estimated "optimum size"
    // (I read somewhere that was 1 to 4 megs?)
    if( !commands.currentTris )
    {
        commands.currentTris = PUSH_RENDER_ELEMENT( commands, RenderEntryTexturedTris );
        commands.currentTris->triCount = 0;
        commands.currentTris->vertexBufferOffset = commands.vertexBuffer.size;
        commands.currentTris->indexBufferOffset = commands.indexBuffer.size;
    }

    RenderEntryTexturedTris *result = commands.currentTris;
    return result;
}

internal void
PushRenderGroup( GameRenderCommands &commands, FlyingDude &dude )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( dude.vertices );
        u32 indexCount = ARRAYCOUNT( dude.indices );

        entry->triCount += indexCount / 3;

        ASSERT( commands.vertexBuffer.size + vertexCount <= commands.vertexBuffer.maxSize );
        TexturedVertex *vert = commands.vertexBuffer.base + commands.vertexBuffer.size;
        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            // TODO Test me!
            // TODO Matrix multiplication should probably be SIMD'd
            vert[i].p = dude.mTransform * dude.vertices[i];
            // TODO Test this!
            vert[i].color = RGBAPack( 255 * V4( 1, 1, 1, 1 ) );
            vert[i].uv = { 0, 0 };
        }
        commands.vertexBuffer.size += vertexCount;

        ASSERT( commands.indexBuffer.size + indexCount <= commands.indexBuffer.maxSize );
        u32 *index = commands.indexBuffer.base + commands.indexBuffer.size;
        for( u32 i = 0; i < indexCount; ++i )
        {
            index[i] = dude.indices[i];
        }
        commands.indexBuffer.size += indexCount;
    }
}

internal void
PushRenderGroup( GameRenderCommands &commands, CubeThing &cube )
{
    //RenderEntryGroup *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryGroup );
    //if( entry )
    //{
        //entry->vertices = cube.vertices;
        //entry->vertexCount = ARRAYCOUNT( cube.vertices );
        //entry->indices = cube.indices;
        //entry->indexCount = ARRAYCOUNT( cube.indices );
        //entry->mTransform = &cube.mTransform;
    //}
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( cube.vertices );
        u32 indexCount = ARRAYCOUNT( cube.indices );

        entry->triCount += indexCount / 3;

        ASSERT( commands.vertexBuffer.size + vertexCount <= commands.vertexBuffer.maxSize );
        TexturedVertex *vert = commands.vertexBuffer.base + commands.vertexBuffer.size;
        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            // TODO Test me!
            // TODO Matrix multiplication should probably be SIMD'd
            vert[i].p = /*cube.mTransform */ cube.vertices[i];
            // TODO Test this!
            vert[i].color = RGBAPack( 255 * V4( 1, 1, 1, 1 ) );
            vert[i].uv = { 0, 0 };
        }
        commands.vertexBuffer.size += vertexCount;

        ASSERT( commands.indexBuffer.size + indexCount <= commands.indexBuffer.maxSize );
        u32 *index = commands.indexBuffer.base + commands.indexBuffer.size;
        for( u32 i = 0; i < indexCount; ++i )
        {
            index[i] = cube.indices[i];
        }
        commands.indexBuffer.size += indexCount;
    }
}
