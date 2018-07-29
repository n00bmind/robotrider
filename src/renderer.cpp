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

#define PUSH_RENDER_ELEMENT(commands, type) (type *)_PushRenderElement( commands, sizeof(type), RenderEntryType::type )
internal RenderEntry *
_PushRenderElement( RenderCommands *commands, u32 size, RenderEntryType type )
{
    RenderEntry *result = 0;
    RenderBuffer &buffer = commands->renderBuffer;

    if( buffer.size + size < buffer.maxSize )
    {
        result = (RenderEntry *)(buffer.base + buffer.size);
        CLEAR0( result, size );
        result->type = type;
        result->size = size;
        buffer.size += size;
    }
    else
    {
        // We don't just do an assert so we're fail tolerant in release builds
        INVALID_CODE_PATH
    }

    // NOTE Reset all batched entries so they start over as needed
    // TODO We will probably need more control over this in the future
    commands->currentTris = nullptr;
    commands->currentLines = nullptr;

    return result;
}

void
PushClear( const v4& color, RenderCommands *commands )
{
    RenderEntryClear *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryClear );
    if( entry )
    {
        entry->color = color;
    }
}

internal RenderEntryTexturedTris *
GetOrCreateCurrentTris( RenderCommands *commands )
{
    // TODO Artificially break the current bundle when we get to an estimated "optimum size"
    // (if there is such a thing.. I read somewhere that was 1 to 4 megs?)
    if( !commands->currentTris )
    {
        commands->currentTris = PUSH_RENDER_ELEMENT( commands, RenderEntryTexturedTris );
        commands->currentTris->vertexBufferOffset = commands->vertexBuffer.count;
        commands->currentTris->indexBufferOffset = commands->indexBuffer.count;
        commands->currentTris->vertexCount = 0;
        commands->currentTris->indexCount = 0;
    }

    RenderEntryTexturedTris *result = commands->currentTris;
    return result;
}

internal RenderEntryLines *
GetOrCreateCurrentLines( RenderCommands *commands )
{
    if( !commands->currentLines )
    {
        commands->currentLines = PUSH_RENDER_ELEMENT( commands, RenderEntryLines );
        commands->currentLines->vertexBufferOffset = commands->vertexBuffer.count;
        commands->currentLines->lineCount = 0;
    }

    RenderEntryLines *result = commands->currentLines;
    return result;
}

inline internal void
PushVertex( const v3 &p, u32 color, const v2 &uv, RenderCommands *commands )
{
    ASSERT( commands->vertexBuffer.count + 1 <= commands->vertexBuffer.maxCount );

    TexturedVertex *vert = commands->vertexBuffer.base + commands->vertexBuffer.count;
    vert->p = p;
    vert->color = color;
    vert->uv = uv;

    commands->vertexBuffer.count++;
}

inline internal void
PushIndex( u32 value, RenderCommands* commands )
{
    ASSERT( commands->indexBuffer.count + 1 <= commands->indexBuffer.maxCount );

    u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
    *index = value;

    commands->indexBuffer.count++;
}

// Push 4 vertices (1st vertex is "top-left" and counter-clockwise from there)
void
PushQuad( const v3 &p1, const v3 &p2, const v3 &p3, const v3 &p4, u32 color, RenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        int indexOffsetStart = entry->vertexCount;

        PushVertex( p1, color, { 0, 0 }, commands );
        PushVertex( p2, color, { 0, 1 }, commands );
        PushVertex( p3, color, { 1, 1 }, commands );
        PushVertex( p4, color, { 1, 0 }, commands );
        entry->vertexCount += 4;

        // Push 6 indices for vertices 0-1-2 & 2-3-0
        PushIndex( indexOffsetStart + 0, commands );
        PushIndex( indexOffsetStart + 1, commands );
        PushIndex( indexOffsetStart + 2, commands );

        PushIndex( indexOffsetStart + 2, commands );
        PushIndex( indexOffsetStart + 3, commands );
        PushIndex( indexOffsetStart + 0, commands );
        entry->indexCount += 6;
    }
}

#if 0
void
PushRenderGroup( FlyingDude *dude, RenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( dude->vertices );
        u32 indexCount = ARRAYCOUNT( dude->indices );

        int indexOffsetStart = entry->vertexCount;

        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( dude->mTransform * dude->vertices[i],
                        Pack01ToRGBA( V4( 1, 0, 0, 1 ) ),
                        { 0, 0 },
                        commands );
        }
        entry->vertexCount += vertexCount;

        for( u32 i = 0; i < indexCount; ++i )
        {
            PushIndex( indexOffsetStart + dude->indices[i], commands );
        }
        entry->indexCount += indexCount;
    }
}

void
PushRenderGroup( CubeThing *cube, RenderCommands *commands )
{
    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        u32 vertexCount = ARRAYCOUNT( cube->vertices );
        u32 indexCount = ARRAYCOUNT( cube->indices );

        int indexOffsetStart = entry->vertexCount;

        for( u32 i = 0; i < vertexCount; ++i )
        {
            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( cube->mTransform * cube->vertices[i],
                        Pack01ToRGBA( V4( 0, 0, 0, 1 ) ),
                        { 0, 0 },
                        commands );
        }
        entry->vertexCount += vertexCount;

        for( u32 i = 0; i < indexCount; ++i )
        {
            PushIndex( indexOffsetStart + cube->indices[i], commands );
        }
        entry->indexCount += indexCount;
    }
}
#endif

void
PushLine( v3 pStart, v3 pEnd, u32 color, RenderCommands *commands )
{
    RenderEntryLines *entry = GetOrCreateCurrentLines( commands );
    if( entry )
    {
        PushVertex( pStart, color, { 0, 0 }, commands );
        PushVertex( pEnd, color, { 0, 0 }, commands );

        ++entry->lineCount;
    }
}

void
PushMesh( const Mesh& mesh, RenderCommands *commands )
{
    TIMED_BLOCK;

    RenderEntryTexturedTris *entry = GetOrCreateCurrentTris( commands );
    if( entry )
    {
        int indexOffsetStart = entry->vertexCount;

        for( u32 i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& v = mesh.vertices[i];

            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( mesh.mTransform * v.p, v.color, v.uv, commands );
        }
        entry->vertexCount += mesh.vertexCount;

        for( u32 i = 0; i < mesh.indexCount; ++i )
        {
            PushIndex( indexOffsetStart + mesh.indices[i], commands );
        }
        entry->indexCount += mesh.indexCount;
    }
}

void
PushProgramChange( ShaderProgramName programName, RenderCommands *commands )
{
    RenderEntryProgramChange *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryProgramChange );
    if( entry )
    {
        entry->programName = programName;
    }
}

void
PushMaterial( Material* material, RenderCommands* commands )
{
    RenderEntryMaterial *entry = PUSH_RENDER_ELEMENT( commands, RenderEntryMaterial );
    if( entry )
    {
        entry->material = material;
    }
}
