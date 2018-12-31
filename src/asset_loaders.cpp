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

Texture
LoadTexture( const char* path, bool flipVertically, bool filtered = true, int desiredChannels = 0 )
{
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );

    stbi_set_flip_vertically_on_load( flipVertically );

    i32 imageWidth = 0, imageHeight = 0, imageChannels = 0;
    u8* imageBuffer =
        stbi_load_from_memory( (u8*)read.contents, read.contentSize,
                               &imageWidth, &imageHeight, &imageChannels, desiredChannels );
    // TODO Async texture uploads (and unload)
    void* textureHandle
        = globalPlatform.AllocateOrUpdateTexture( imageBuffer, imageWidth, imageHeight, filtered, nullptr );

    globalPlatform.DEBUGFreeFileMemory( read.contents );

    Texture result;
    result.handle = textureHandle;
    result.imageBuffer = imageBuffer;
    result.width = imageWidth;
    result.height = imageHeight;
    result.channelCount = imageChannels;

    return result;
}

Array<WFC::Spec>
LoadWFCVars( const char* path, MemoryArena* arena, const TemporaryMemory& tempMemory )
{
    Array<WFC::Spec> empty;
    BucketArray<WFC::Spec> result( tempMemory.arena, 128, Temporary() );

    const u32 minVersionNumber = 1;


    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );
    ASSERT( read.contents );

    String contents( (char*)read.contents );

    String firstLine = contents.ConsumeLine();
    String firstWord = firstLine.ConsumeWord();

    // First line must be version number
    if( !firstWord.IsEqual( "#" ) )
    {
        LOG( "ERROR :: First line must be a comment specifying 'version X' for the version number" );
        return empty;
    }

    firstWord.Consume( 1 );
    if( !firstWord )
        firstWord = firstLine.ConsumeWord();

    if( !firstWord.IsEqual( "version", false ) )
    {
        LOG( "ERROR :: First line must be a comment specifying 'version X' for the version number" );
        return empty;
    }

    String version = firstLine.ConsumeWord();
    i32 versionNumber;
    if( !version.ToI32( &versionNumber ) )
    {
        LOG( "ERROR :: version argument '%s' is not an integer number",
             version.CString( tempMemory.arena ), Temporary() );
        return empty;
    }

    if( versionNumber < minVersionNumber )
    {
        LOG( "ERROR :: WFC vars file %s has version %d, minimum supported version is %d", 
             path, versionNumber, minVersionNumber );
        return empty;
    }

    u32 currentLineNumber = 2;
    WFC::Spec* currentSpec = nullptr;
    bool specHasName = false;
    bool specHasSource = false;
    String specSourceImage;

    while( contents )
    {
        String line = contents.ConsumeLine();

        if( line.IsNullOrEmpty() )
        {
            if( currentSpec )
            {
                if( specHasName && specHasSource )
                {
                    currentSpec->source = LoadTexture( specSourceImage.CString( tempMemory.arena, Temporary() ), 
                                                       false, false, 4 );
                }
                else
                {
                    LOG( "ERROR :: WFC spec requires at least 'name' and 'sourceImage' attributes at line %d", 
                         currentLineNumber );
                    *currentSpec = WFC::DefaultSpec();
                }
            }

            currentSpec = nullptr;
            specHasName = false;
            specHasSource = false;
            continue;
        }

        String var = line.ConsumeWord();

        if( var.StartsWith( "#" ) )
            ; // Ignore
        else
        {
            if( !currentSpec )
                currentSpec = result.Add( WFC::DefaultSpec() );

            if( var.IsEqual( "name", false ) )
            {
                String nameString = line.ScanString();
                if( nameString.IsNullOrEmpty() )
                {
                    LOG( "ERROR :: Missing 'name' string argument at line %d", currentLineNumber );
                    currentSpec->name = "unknown";
                }
                else
                    currentSpec->name = nameString.CString( arena );

                specHasName = true;
            }
            else if( var.IsEqual( "sourceImage", false ) )
            {
                String imageString = line.ScanString();
                if( imageString.IsNullOrEmpty() )
                {
                    LOG( "ERROR :: Missing 'sourceImage' string argument at line %d", currentLineNumber );
                }
                else
                {
                    specSourceImage = imageString;
                    specHasSource = true;
                }
            }
            else if( var.IsEqual( "N", false ) )
            {
                String nWord = line.ConsumeWord();

                u32 n;
                if( !nWord || !nWord.ToU32( &n ) )
                    LOG( "ERROR :: Argument to 'N' must be an unsigned integer at line %d", currentLineNumber );
                else
                    currentSpec->N = n;
            }
            else if( var.IsEqual( "outputDim", false ) )
            {
                String xWord = line.ConsumeWord();
                u32 x;
                if( !xWord || !xWord.ToU32( &x ) )
                    LOG( "ERROR :: Argument to 'outputDim' must be a vector2 of unsigned integer at line %d", 
                         currentLineNumber );
                else
                {
                    String yWord = line.ConsumeWord();
                    u32 y;
                    if( !yWord || !yWord.ToU32( &y ) )
                        LOG( "ERROR :: Argument to 'outputDim' must be a vector2 of unsigned integer at line %d",
                             currentLineNumber );
                    else
                        currentSpec->outputChunkDim = V2u( x, y );
                }
            }
            else if( var.IsEqual( "periodic", false ) )
            {
                String boolWord = line.ConsumeWord();
                bool value;
                if( !boolWord || !boolWord.ToBool( &value ) )
                    LOG( "ERROR :: Argument to 'periodic' must be a boolean at line %d", currentLineNumber );
                else
                    currentSpec->periodic = value;
            }
            else
            {
                LOG( "WARNING :: Unknown var '%s' ignored at line %d", var.CString( tempMemory.arena, Temporary() ),
                     currentLineNumber );
            }
        }

        currentLineNumber++;
    }

    if( currentSpec )
    {
        if( specHasName && specHasSource )
        {
            currentSpec->source = LoadTexture( specSourceImage.CString( tempMemory.arena, Temporary() ),
                                               false, false, 4 );
        }
        else
        {
            LOG( "ERROR :: WFC spec requires at least 'name' and 'sourceImage' attributes at line %d",
                 currentLineNumber );
            result.Pop();
        }
    }

    return result.ToArray( arena );
}


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

inline u32
VertexHash( const VertexKey& key, u32 tableSize )
{
    return key.pIdx;
}

Mesh
LoadOBJ( const char* path, MemoryArena* arena, const TemporaryMemory& tmpMemory,
         const m4& appliedTransform = M4Identity )
{
    // TODO Centralize asset loading in the platform layer (through a 'bundle' file),
    // so that temporary memory used while loading is reused and not allocated every time
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );
    ASSERT( read.contents );

    u32 vertexCount = 0, indexCount = 0, uvCount = 0, normalCount = 0;

    String contents( (char *)read.contents );
    while( contents )
    {
        String line = contents.ConsumeLine();
        if( line.IsNullOrEmpty() )
            continue;

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

    Array<u32> indices( arena, 0, indexCount );

    Array<v3> positions( tmpMemory.arena, 0, vertexCount, Temporary() );
    Array<v2> uvs( tmpMemory.arena, 0, uvCount, Temporary() );
    Array<v3> normals( tmpMemory.arena, 0, normalCount, Temporary() );

    // Map vertex pos+uv+normal to final vertex index
    // Initially every vertex would map to exactly one entry in the table, but due to
    // the way face data is specified, we may need to spawn additional vertices
    // that will share the same position but differ in tex coord and/or normal
    u32 tableSize = NextPowerOf2( vertexCount );
    HashTable<VertexKey, u32, VertexHash> cachedVertices( tmpMemory.arena, tableSize, Temporary() );
    BucketArray<TexturedVertex> vertices( tmpMemory.arena, 1024, Temporary() );

    contents = (char*)read.contents;
    while( contents )
    {
        String line = contents.ConsumeLine();
        if( line.IsNullOrEmpty() )
            continue;

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
            positions.Push( p );
        }
        else if( firstWord.IsEqual( "vt" ) )
        {
            v2 uv;
            int matches = line.Scan( "%f %f", &uv.u, &uv.v );
            ASSERT( matches == 2 );
            uvs.Push( uv );
        }
        else if( firstWord.IsEqual( "vn" ) )
        {
            v3 n;
            int matches = line.Scan( "%f %f %f", &n.x, &n.y, &n.z );
            ASSERT( matches == 3 );
            Normalize( n );
            normals.Push( n );
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
                    cachedVertices.Add( key, vertexIndex[i] );
                }

                i++;
                word = line.ConsumeWord();
            }

            u32 polyVertices = i;
            ASSERT( polyVertices == 3 || polyVertices == 4 );
            
            indices.Push( vertexIndex[0] );
            indices.Push( vertexIndex[1] );
            indices.Push( vertexIndex[2] );

            if( polyVertices == 4 )
            {
                indices.Push( vertexIndex[2] );
                indices.Push( vertexIndex[3] );
                indices.Push( vertexIndex[0] );
            }
        }
    }

    globalPlatform.DEBUGFreeFileMemory( read.contents );

    Array<TexturedVertex> packedVertices( arena, 0, vertices.count );
    vertices.CopyTo( packedVertices.data );
    packedVertices.count = vertices.count;

    // Pre apply transform
    if( !AlmostEqual( appliedTransform, M4Identity ) )
    {
        for( u32 i = 0; i < packedVertices.count; ++i )
            packedVertices.data[i].p = appliedTransform * packedVertices.data[i].p;
    }

    Mesh result;
    Init( &result );
    result.vertices = packedVertices.data;
    result.vertexCount = packedVertices.count;
    result.indices = indices.data;
    result.indexCount = indices.count;
    CalcBounds( &result );

    return result;
}

