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
        ZERO( result, size );
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
    TIMED_BLOCK;

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
    TIMED_BLOCK;

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
#if 0
        for( u32 i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& v = mesh.vertices[i];

            // Transform to world coordinates so this can all be rendered in big chunks
            PushVertex( mesh.mTransform * v.p, v.color, v.uv, commands );
        }
        int indexOffsetStart = entry->vertexCount;
        entry->vertexCount += mesh.vertexCount;

        for( u32 i = 0; i < mesh.indexCount; ++i )
        {
            PushIndex( indexOffsetStart + mesh.indices[i], commands );
        }
        entry->indexCount += mesh.indexCount;
#else
        ASSERT( commands->vertexBuffer.count + mesh.vertexCount <= commands->vertexBuffer.maxCount );

        //const __m128 r0 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 0 );
        //const __m128 r1 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 4 );
        //const __m128 r2 = _mm_loadu_ps( ((r32*)&mesh.mTransform) + 8 );
        for( u32 i = 0; i < mesh.vertexCount; ++i )
        {
            TexturedVertex& src = mesh.vertices[i];
            TexturedVertex *dst = commands->vertexBuffer.base + commands->vertexBuffer.count;

            // TODO Redesign Mesh structure as a SOA so that we can apply more efficient copies and transforms like
            // http://fastcpp.blogspot.com/2011/07/fast-3d-vector-matrix-transformation.html
            dst->p = Transform( mesh.mTransform, src.p );
            //v4 pSrc = V4( src.p, 1 );
            //__m128 xyz1 = _mm_loadu_ps( (r32*)&pSrc );
            //__m128 x___ = _mm_dp_ps( r0, xyz1, 0xF1 );
            //__m128 _y__ = _mm_dp_ps( r1, xyz1, 0xF2 );
            //__m128 __z_ = _mm_dp_ps( r2, xyz1, 0xF4 );
            //__m128 xy__ = _mm_add_ps( x___, _y__ );
            //__m128 xyz_ = _mm_add_ps( xy__, __z_ );
            //_mm_storeu_ps( (r32*)&dst->p, xyz_ );

            dst->color = src.color;
            dst->uv = src.uv;

            commands->vertexBuffer.count++;
        }
        int indexOffsetStart = entry->vertexCount;
        entry->vertexCount += mesh.vertexCount;

        ASSERT( commands->indexBuffer.count + mesh.indexCount <= commands->indexBuffer.maxCount );

        for( u32 i = 0; i < mesh.indexCount; ++i )
        {
            u32 *index = commands->indexBuffer.base + commands->indexBuffer.count;
            *index = indexOffsetStart + mesh.indices[i];

            commands->indexBuffer.count++;
        }
        entry->indexCount += mesh.indexCount;
#endif
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

internal void
DrawBounds( const aabb& box, u32 color, RenderCommands* renderCommands )
{
    PushLine( V3( box.xMin, box.yMin, box.zMin ), V3( box.xMax, box.yMin, box.zMin ), color, renderCommands );
    PushLine( V3( box.xMin, box.yMax, box.zMin ), V3( box.xMax, box.yMax, box.zMin ), color, renderCommands );
    PushLine( V3( box.xMin, box.yMin, box.zMin ), V3( box.xMin, box.yMax, box.zMin ), color, renderCommands );
    PushLine( V3( box.xMax, box.yMin, box.zMin ), V3( box.xMax, box.yMax, box.zMin ), color, renderCommands );

    PushLine( V3( box.xMin, box.yMin, box.zMin ), V3( box.xMin, box.yMin, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMax, box.yMin, box.zMin ), V3( box.xMax, box.yMin, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMin, box.yMax, box.zMin ), V3( box.xMin, box.yMax, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMax, box.yMax, box.zMin ), V3( box.xMax, box.yMax, box.zMax ), color, renderCommands );

    PushLine( V3( box.xMin, box.yMin, box.zMax ), V3( box.xMax, box.yMin, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMin, box.yMax, box.zMax ), V3( box.xMax, box.yMax, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMin, box.yMin, box.zMax ), V3( box.xMin, box.yMax, box.zMax ), color, renderCommands );
    PushLine( V3( box.xMax, box.yMin, box.zMax ), V3( box.xMax, box.yMax, box.zMax ), color, renderCommands );
}

internal void
DrawBoxAt( const v3& p, r32 size, u32 color, RenderCommands* renderCommands )
{
    aabb box =
    {
        p.x - size / 2, p.x + size / 2,
        p.y - size / 2, p.y + size / 2,
        p.z - size / 2, p.z + size / 2,
    };
    DrawBounds( box, color, renderCommands );
}

internal void
DrawFloorGrid( r32 areaSizeMeters, r32 resolutionMeters, RenderCommands* renderCommands )
{
    const r32 areaHalf = areaSizeMeters / 2;

    u32 semiBlack = Pack01ToRGBA( V4( 0, 0, 0, 0.1f ) );
    v3 off = V3Zero;

    r32 yStart = -areaHalf;
    r32 yEnd = areaHalf;
    for( float x = -areaHalf; x <= areaHalf; x += resolutionMeters )
    {
        PushLine( V3( x, yStart, 0 ) + off, V3( x, yEnd, 0 ) + off, semiBlack, renderCommands );
    }
    r32 xStart = -areaHalf;
    r32 xEnd = areaHalf;
    for( float y = -areaHalf; y <= areaHalf; y += resolutionMeters )
    {
        PushLine( V3( xStart, y, 0 ) + off, V3( xEnd, y, 0 ) + off, semiBlack, renderCommands );
    }
}

internal void
DrawCubicGrid( const aabb& boundingBox, r32 step, u32 color, bool drawZAxis, RenderCommands* renderCommands )
{
    ASSERT( step > 0.f );

    r32 xMin = boundingBox.xMin;
    r32 xMax = boundingBox.xMax;
    r32 yMin = boundingBox.yMin;
    r32 yMax = boundingBox.yMax;
    r32 zMin = boundingBox.zMin;
    r32 zMax = boundingBox.zMax;

    for( r32 z = zMin; z <= zMax; z += step )
    {
        for( r32 y = yMin; y <= yMax; y += step )
            PushLine( { xMin, y, z }, { xMax, y, z }, color, renderCommands );

        for( r32 x = xMin; x <= xMax; x += step )
            PushLine( { x, yMin, z }, { x, yMax, z }, color, renderCommands );
    }

    if( drawZAxis )
    {
        for( r32 y = yMin; y <= yMax; y += step )
        {
            for( r32 x = xMin; x <= xMax; x += step )
                PushLine( { x, y, zMin }, { x, y, zMax }, color, renderCommands );
        }
    }
}

