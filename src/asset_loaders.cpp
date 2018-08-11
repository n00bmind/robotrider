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

struct VertexKey
{
    u32 pIdx;
    u32 uvIdx;
    u32 nIdx;

    bool operator==( const VertexKey& other ) const
    {
        return this->pIdx == other.pIdx &&
            this->uvIdx == other.uvIdx &&
            this->nIdx == other.nIdx;
    }
};

u32
VertexHash( const VertexKey& key )
{
    return key.pIdx;
}

Mesh
LoadOBJ( const char* path, MemoryArena* arena, MemoryArena* tmpArena,
         const m4& appliedTransform = M4Identity() )
{
    // TODO Centralize asset loading in the platform layer (through a 'bundle' file),
    // so that temporary memory used while loading is reused and not allocated every time
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );
    ASSERT( read.contents );

    u32 vertexCount = 0, indexCount = 0, uvCount = 0, normalCount = 0;

    String contents( (char *)read.contents );
    while( true )
    {
        String line = contents.ConsumeLine();
        if( line.IsNullOrEmpty() )
            break;

        // This trims all initial whitespace
        String firstWord = line.ConsumeWord();

        // Do a quick first pass just to count everything
        if( firstWord.StartsWith( "#" ) )
            ;
        else if( firstWord.IsEqual( "v" ) )
            vertexCount++;
        else if( firstWord.IsEqual( "vt" ) )
            uvCount++;
        else if( firstWord.IsEqual( "vn" ) )
            normalCount++;
        else if( firstWord.IsEqual( "f" ) )
        {
            u32 polyVertices = 0;
            String word = line.ConsumeWord();
            while( word )
            {
                polyVertices++;
                word = line.ConsumeWord();
            }
            // Assume quads if more than 3 vertices
            indexCount += (polyVertices == 3) ? 3 : 6;
        }
    }

    Array<u32> indices( arena, indexCount );

    Array<v3> positions( tmpArena, vertexCount );
    Array<v2> uvs( tmpArena, uvCount );
    Array<v3> normals( tmpArena, normalCount );

    // Map vertex pos+uv+normal to final vertex index
    // Initially every vertex would map to exactly one entry in the table, but due to
    // the way face data is specified, we may need to spawn additional vertices
    // that will share the same position but differ in tex coord and/or normal
    u32 tableSize = GetNextPowerOf2( vertexCount );
    HashTable<VertexKey, u32, VertexHash> cachedVertices( tmpArena, tableSize );
    BucketArray<TexturedVertex> vertices( 1024, tmpArena );

    contents = (char*)read.contents;
    while( true )
    {
        String line = contents.ConsumeLine();
        if( line.IsNullOrEmpty() )
            break;

        String firstWord = line.ConsumeWord();

        if( firstWord.StartsWith( "#" ) )
        {
            // Skip comments
        }
        else if( firstWord.IsEqual( "v" ) )
        {
            v3 p;
            int matches = line.Scan( "%f %f %f", &p.x, &p.y, &p.z );
            ASSERT( matches == 3 );
            positions.Add( p );
        }
        else if( firstWord.IsEqual( "vt" ) )
        {
            v2 uv;
            int matches = line.Scan( "%f %f", &uv.u, &uv.v );
            ASSERT( matches == 2 );
            uvs.Add( uv );
        }
        else if( firstWord.IsEqual( "vn" ) )
        {
            v3 n;
            int matches = line.Scan( "%f %f %f", &n.x, &n.y, &n.z );
            ASSERT( matches == 3 );
            Normalize( n );
            normals.Add( n );
        }
        else if( firstWord.IsEqual( "f" ) )
        {
            u32 vertexIndex[4] = {};
            i32 pIndex[4] = {}, uvIndex[4] = {}, nIndex[4] = {};
            bool haveUVs = false, haveNormals = false;

            u32 i = 0;

            // NOTE Since uvs and normals are not associated with vertices but with faces,
            // we cache constructed vertices in the hash table to know whether future
            // vertices with the same position can be reused or will need a new entry
            // in the final vertex array
            String word = line.ConsumeWord();
            while( word )
            {
                ASSERTM( i < 4, "Polys with more than 4 vertices are not supported!" );

                if( word.Scan( "%d/%d/%d", &pIndex[i], &uvIndex[i], &nIndex[i] ) == 3 )
                {
                    haveUVs = true;
                    haveNormals = true;
                }
                else if( word.Scan( "%d/%d", &pIndex[i], &uvIndex[i] ) == 2 )
                {
                    haveUVs = true;
                }
                else if( word.Scan( "%d//%d", &pIndex[i], &nIndex[i] ) == 2 )
                {
                    haveNormals = true;
                }
                else if( word.Scan( "%d", &pIndex[i] ) == 1 )
                {
                }
                else
                {
                    ASSERTM( false, "I don't understand this OBJ file!" );
                }

                // All indices start at 1
                // Vertex indices could be in relative (negative) form
                if( pIndex[i] > 0 )
                    pIndex[i]--;
                else
                    pIndex[i] = vertices.count - pIndex[i];

                if( haveUVs )
                    uvIndex[i]--;
                if( haveNormals )
                    nIndex[i]--;

                // Try to fetch an existing vertex from the table
                VertexKey key = { (u32)pIndex[i], (u32)uvIndex[i], (u32)nIndex[i] };
                u32* cachedIndex = cachedVertices.Find( key );

                if( cachedIndex )
                {
#if !RELEASE
                    TexturedVertex& vertex = vertices[*cachedIndex];
                    ASSERT( vertex.p == positions[key.pIdx] );
                    if( haveUVs )
                        ASSERT( vertex.uv == uvs[key.uvIdx] );
                    if( haveNormals )
                        ASSERT( vertex.n == normals[key.nIdx] );
#endif
                    vertexIndex[i] = *cachedIndex;
                }
                else
                {
                    v2 uv = {};
                    v3 n = {};

                    if( haveUVs )
                        uv = uvs[key.uvIdx];
                    if( haveNormals )
                        n = normals[key.nIdx];

                    TexturedVertex newVertex =
                    {
                        positions[key.pIdx],
                        Pack01ToRGBA( 1, 1, 1, 1 ),
                        uv,
                        n,
                    };
                    vertexIndex[i] = vertices.count;
                    vertices.Add( newVertex );
                    cachedVertices.Add( key, vertexIndex[i], tmpArena );
                }

                i++;
                word = line.ConsumeWord();
            }

            u32 polyVertices = i;
            ASSERT( polyVertices == 3 || polyVertices == 4 );
            
            indices.Add( vertexIndex[0] );
            indices.Add( vertexIndex[1] );
            indices.Add( vertexIndex[2] );

            if( polyVertices == 4 )
            {
                indices.Add( vertexIndex[2] );
                indices.Add( vertexIndex[3] );
                indices.Add( vertexIndex[0] );
            }
        }
    }

    globalPlatform.DEBUGFreeFileMemory( read.contents );

    Array<TexturedVertex> packedVertices( arena, vertices.count );
    vertices.BlitTo( packedVertices.data );
    packedVertices.count = vertices.count;

    // Pre apply transform
    if( !AlmostEqual( appliedTransform, M4Identity() ) )
    {
        for( u32 i = 0; i < packedVertices.count; ++i )
            packedVertices.data[i].p = appliedTransform * packedVertices.data[i].p;
    }

    Mesh result = {};
    result.vertices = packedVertices.data;
    result.vertexCount = packedVertices.count;
    result.indices = indices.data;
    result.indexCount = indices.count;

    return result;
}

LoadTextureResult
LoadTexture( const char* path )
{
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );

    i32 imageWidth = 0, imageHeight = 0, imageChannels = 0;

    // OpenGL has weird coords!
    stbi_set_flip_vertically_on_load( true );

    // TODO Use our allocators!
    u8* imageBuffer =
        stbi_load_from_memory( (u8*)read.contents, read.contentSize,
                               &imageWidth, &imageHeight, &imageChannels, 4 );
    // TODO Async texture uploads (and unload)
    void* textureHandle
        = globalPlatform.AllocateTexture( imageBuffer, imageWidth, imageHeight );

    globalPlatform.DEBUGFreeFileMemory( read.contents );

    LoadTextureResult result;
    result.textureHandle = textureHandle;
    result.imageBuffer = imageBuffer;

    return result;
}
