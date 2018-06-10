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

    Array<TexturedVertex> vertices( arena, vertexCount );
    Array<u32> indices( arena, indexCount );
    Array<v2> uvs( tmpArena, uvCount );
    Array<v3> normals( tmpArena, normalCount );

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
            TexturedVertex vertex = {};
            vertex.color = Pack01ToRGBA( { 0, 0, 0, 1 } );

            int matches = line.Scan( "%f %f %f", &vertex.p.x, &vertex.p.y, &vertex.p.z );
            ASSERT( matches == 3 );
            vertices.Add( vertex );
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
            v3 normal;
            int matches = line.Scan( "%f %f %f", &normal.x, &normal.y, &normal.z );
            ASSERT( matches == 3 );
            Normalize( normal );
            normals.Add( normal );
        }
        else if( firstWord.IsEqual( "f" ) )
        {
            i32 vertexIndex[4], uvIndex[4], normalIndex[4];
            bool haveUVs = false, haveNormals = false;

            u32 i = 0;
            // NOTE Since uvs and normals are not associated with vertices but with faces,
            // we need to do a de-indexing step. There's the open question of what to do
            // if/when indices don't have consistency...
            String word = line.ConsumeWord();
            while( word )
            {
                ASSERTM( i < 4, "Polys with more than 4 vertices are not supported!" );

                if( word.Scan( "%d/%d/%d", &vertexIndex[i], &uvIndex[i], &normalIndex[i] ) == 3 )
                {
                    haveUVs = true;
                    haveNormals = true;
                }
                else if( word.Scan( "%d/%d", &vertexIndex[i], &uvIndex[i] ) == 2 )
                {
                    haveUVs = true;
                }
                else if( word.Scan( "%d//%d", &vertexIndex[i], &normalIndex[i] ) == 2 )
                {
                    haveNormals = true;
                }
                else if( word.Scan( "%d", &vertexIndex[i] ) == 1 )
                {
                }

                else
                {
                    ASSERTM( false, "I don't understand this OBJ file!" );
                }

                i++;
                word = line.ConsumeWord();
            }

            u32 polyVertices = i;
            ASSERT( polyVertices == 3 || polyVertices == 4 );
            
            // Vertex indices could be in relative (negative) form
            if( vertexIndex[0] > 0 )
            {
                // We assume if one is positive, all are, and vice-versa
                vertexIndex[0]--;
                vertexIndex[1]--;
                vertexIndex[2]--;
                vertexIndex[3]--;
            }
            else
            {
                vertexIndex[0] = vertices.count - vertexIndex[0];
                vertexIndex[1] = vertices.count - vertexIndex[1];
                vertexIndex[2] = vertices.count - vertexIndex[2];
                vertexIndex[3] = vertices.count - vertexIndex[3];
            }

            indices.Add( vertexIndex[0] );
            indices.Add( vertexIndex[1] );
            indices.Add( vertexIndex[2] );

            if( polyVertices == 4 )
            {
                indices.Add( vertexIndex[2] );
                indices.Add( vertexIndex[3] );
                indices.Add( vertexIndex[0] );
            }

            TexturedVertex& vert0 = vertices[vertexIndex[0]];
            TexturedVertex& vert1 = vertices[vertexIndex[1]];
            TexturedVertex& vert2 = vertices[vertexIndex[2]];

            if( haveUVs )
            {
                // All indices start at 1
                uvIndex[0]--;
                uvIndex[1]--;
                uvIndex[2]--;
                uvIndex[3]--;

                vert0.uv = uvs[uvIndex[0]];
                vert1.uv = uvs[uvIndex[1]];
                vert2.uv = uvs[uvIndex[2]];
            }
            if( haveNormals )
            {
                normalIndex[0]--;
                normalIndex[1]--;
                normalIndex[2]--;
                normalIndex[3]--;

                vert0.normal = normals[normalIndex[0]];
                vert1.normal = normals[normalIndex[1]];
                vert2.normal = normals[normalIndex[2]];
            }

            if( polyVertices == 4 )
            {
                TexturedVertex& vert3 = vertices[vertexIndex[3]];

                if( haveUVs )
                    vert3.uv = uvs[uvIndex[3]];
                if( haveNormals )
                    vert3.normal = normals[normalIndex[3]];
            }
        }
    }

    globalPlatform.DEBUGFreeFileMemory( read.contents );

    // Pre apply transform
    if( !AlmostEqual( appliedTransform, M4Identity() ) )
    {
        for( u32 i = 0; i < vertices.count; ++i )
            vertices.data[i].p = appliedTransform * vertices.data[i].p;
    }

    Mesh result = {};
    result.vertices = vertices.data;
    result.vertexCount = vertices.count;
    result.indices = indices.data;
    result.indexCount = indices.count;

    return result;
}

void*
LoadTexture( const char* path )
{
    DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( path );

    i32 imageWidth = 0, imageHeight = 0, imageChannels = 0;

    u8* imageBuffer =
        stbi_load_from_memory( (u8*)read.contents, read.contentSize,
                               &imageWidth, &imageHeight, &imageChannels, 4 );

    // TODO Async texture uploads (and unload)
    globalPlatform.AllocateTexture( imageBuffer, imageWidth, imageHeight );

    stbi_image_free( imageBuffer );
    globalPlatform.DEBUGFreeFileMemory( read.contents );

    return imageBuffer;
}
