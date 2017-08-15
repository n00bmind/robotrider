
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
    //RenderEntryGroup *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryGroup );
    //if( entry )
    //{
        //entry->vertices = dude.vertices;
        //entry->vertexCount = ARRAYCOUNT( dude.vertices );
        //entry->indices = dude.indices;
        //entry->indexCount = ARRAYCOUNT( dude.indices );
        //entry->mTransform = &dude.mTransform;
    //}

    RenderEntryTexturedTri *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryTexturedTri );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( dude.vertices );
        u32 indexCount = ARRAYCOUNT( dude.indices );

        entry->vertexArrayOffset = commands.vertexBuffer.size;
        entry->indexArrayOffset = commands.indexBuffer.size;
        // Material **materialArray;
        entry->triCount = indexCount / 3;

        ASSERT( commands.vertexBuffer.size + vertexCount <= commands.vertexBuffer.maxSize );
        TexturedVertex *vert = commands.vertexBuffer.base + commands.vertexBuffer.size;
        for( u32 i = 0; i < vertexCount; ++i )
        {
            vert[i].p = dude.vertices[i];
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
