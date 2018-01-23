
#define PUSH_RENDER_ELEMENT(commands, type) (type *)_PushRenderElement( commands, sizeof(type), RenderEntryType::type )
internal RenderEntry *
_PushRenderElement( GameRenderCommands *commands, u32 size, RenderEntryType type )
{
    RenderEntry *result = 0;
    RenderBuffer &buffer = commands->renderBuffer;

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
    commands->currentTris = 0;

    return result;
}

void
PushClear( GameRenderCommands *commands, v4 color )
{
    RenderEntryClear *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryClear );
    if( entry )
    {
        entry->color = color;
    }
}

internal RenderEntryTexturedTris *
GetOrCreateCurrentTris( GameRenderCommands *commands )
{
    // FIXME Artificially break the current bundle when we get to an estimated "optimum size"
    // (I read somewhere that was 1 to 4 megs?)
    if( !commands->currentTris )
    {
        commands->currentTris = PUSH_RENDER_ELEMENT( commands, RenderEntryTexturedTris );
        commands->currentTris->triCount = 0;
        commands->currentTris->vertexBufferOffset = commands->vertexBuffer.count;
        commands->currentTris->indexBufferOffset = commands->indexBuffer.count;
    }

    RenderEntryTexturedTris *result = commands->currentTris;
    return result;
}

void
PushQuad( GameRenderCommands *commands, const v3 &p1, const v3 &p2, const v3 &p3, const v3 &p4, u32 color )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        entry->triCount += 2;

        // Push 4 vertices (1st vertex is "top-left" and counter-clockwise from there)
        TexturedVertex *vert = commands->vertexBuffer.base + commands->vertexBuffer.count;

        u32 vertexCount = 0;
        {
            vert[vertexCount].p = p1;
            vert[vertexCount].color = color;
            vert[vertexCount].uv = { 0, 0 };
        }
        vertexCount++;
        {
            vert[vertexCount].p = p2;
            vert[vertexCount].color = color;
            vert[vertexCount].uv = { 0, 0 };
        }
        vertexCount++;
        {
            vert[vertexCount].p = p3;
            vert[vertexCount].color = color;
            vert[vertexCount].uv = { 0, 0 };
        }
        vertexCount++;
        {
            vert[vertexCount].p = p4;
            vert[vertexCount].color = color;
            vert[vertexCount].uv = { 0, 0 };
        }
        vertexCount++;

        ASSERT( commands->vertexBuffer.count + vertexCount <= commands->vertexBuffer.maxCount );
        int indexOffset = commands->vertexBuffer.count;
        commands->vertexBuffer.count += vertexCount;

        // Push 6 indices for vertices 0-1-2 & 2-3-0
        u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;

        u32 indexCount = 0;
        index[indexCount++] = indexOffset + 0;
        index[indexCount++] = indexOffset + 1;
        index[indexCount++] = indexOffset + 2;

        index[indexCount++] = indexOffset + 2;
        index[indexCount++] = indexOffset + 3;
        index[indexCount++] = indexOffset + 0;

        ASSERT( commands->indexBuffer.count + indexCount <= commands->indexBuffer.maxCount );
        commands->indexBuffer.count += indexCount;
    }
}

void
PushRenderGroup( FlyingDude *dude, GameRenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( dude->vertices );
        u32 indexCount = ARRAYCOUNT( dude->indices );

        entry->triCount += indexCount / 3;

        ASSERT( commands->vertexBuffer.count + vertexCount <= commands->vertexBuffer.maxCount );
        TexturedVertex *vert = commands->vertexBuffer.base + commands->vertexBuffer.count;
        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            // TODO Matrix multiplication should probably be SIMD'd
            vert[i].p = dude->mTransform * dude->vertices[i];
            // TODO Test this!
            vert[i].color = RGBAPack( 255 * V4( 1, 0, 0, 1 ) );
            vert[i].uv = { 0, 0 };
        }
        int indexOffset = commands->vertexBuffer.count;
        commands->vertexBuffer.count += vertexCount;

        ASSERT( commands->indexBuffer.count + indexCount <= commands->indexBuffer.maxCount );
        u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
        for( u32 i = 0; i < indexCount; ++i )
        {
            index[i] = indexOffset + dude->indices[i];
        }
        commands->indexBuffer.count += indexCount;
    }

}

void
PushRenderGroup( CubeThing *cube, GameRenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( cube->vertices );
        u32 indexCount = ARRAYCOUNT( cube->indices );

        entry->triCount += indexCount / 3;

        ASSERT( commands->vertexBuffer.count + vertexCount <= commands->vertexBuffer.maxCount );
        TexturedVertex *vert = commands->vertexBuffer.base + commands->vertexBuffer.count;
        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            // TODO Matrix multiplication should probably be SIMD'd
            vert[i].p = cube->mTransform * cube->vertices[i];
            // TODO Test this!
            vert[i].color = RGBAPack( 255 * V4( 0, 0, 0, 1 ) );
            vert[i].uv = { 0, 0 };
        }
        int indexOffset = commands->vertexBuffer.count;
        commands->vertexBuffer.count += vertexCount;

        ASSERT( commands->indexBuffer.count + indexCount <= commands->indexBuffer.maxCount );
        u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
        for( u32 i = 0; i < indexCount; ++i )
        {
            index[i] = indexOffset + cube->indices[i];
        }
        commands->indexBuffer.count += indexCount;
    }
}
