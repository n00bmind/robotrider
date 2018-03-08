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

// FIXME
internal SArray<v2, 65536> uvs;
internal SArray<v3, 65536> normals;

void
LoadOBJ( const char* path, Array<TexturedVertex>* vertices, Array<u32>* indices )
{
    // TODO Centralize asset loading in the platform layer (through a 'bundle' file),
    // so that temporary memory used while loading is reused and not allocated every time
    DEBUGReadFileResult result = globalPlatform.DEBUGReadEntireFile( path );
    ASSERT( result.contents );

    String contents( (char *)result.contents );
    while( true )
    {
        String line = contents.ConsumeLine();
        if( line.IsNullOrEmpty() )
            break;

        // This trims all whitespace
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
            vertices->Add( vertex );
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
            u32 matches;
            i32 vertexIndex[3], uvIndex[3], normalIndex[3];
            bool haveUVs = false, haveNormals = false;

            // NOTE Since uvs and normals are not associated with vertices but with faces,
            // we need to do a de-indexing step. There's the open question of what to do
            // if/when indices don't have consistency...
            if( (matches = line.Scan( "%d/%d/%d %d/%d/%d %d/%d/%d",
                                      &vertexIndex[0], &uvIndex[0], &normalIndex[0],
                                      &vertexIndex[1], &uvIndex[1], &normalIndex[1],
                                      &vertexIndex[2], &uvIndex[2], &normalIndex[2] )) == 9 )
            {
                haveUVs = true;
                haveNormals = true;
            }
            else if( (matches = line.Scan( "%d/%d %d/%d %d/%d",
                                           &vertexIndex[0], &uvIndex[0],
                                           &vertexIndex[1], &uvIndex[1],
                                           &vertexIndex[2], &uvIndex[2] )) == 6 )
            {
                haveUVs = true;
            }
            else if( (matches = line.Scan( "%d//%d %d//%d %d//%d",
                                           &vertexIndex[0], &normalIndex[0],
                                           &vertexIndex[1], &normalIndex[1],
                                           &vertexIndex[2], &normalIndex[2] )) == 6 )
            {
                haveNormals = true;
            }
            else if( (matches = line.Scan( "%d %d %d",
                                           &vertexIndex[0],
                                           &vertexIndex[1],
                                           &vertexIndex[2] )) == 3 )
            {
                // Nothing else to do
            }
            else
            {
                ASSERTM( false,
                        "We need a better understanding of OBJ format!" );
            }

            TexturedVertex& vert0 = (*vertices)[vertexIndex[0]];
            TexturedVertex& vert1 = (*vertices)[vertexIndex[1]];
            TexturedVertex& vert2 = (*vertices)[vertexIndex[2]];

            if( haveUVs )
            {
                // All indices start at 1
                uvIndex[0]--;
                uvIndex[1]--;
                uvIndex[2]--;
                vert0.uv = uvs[uvIndex[0]];
                vert1.uv = uvs[uvIndex[1]];
                vert2.uv = uvs[uvIndex[2]];
            }
            if( haveNormals )
            {
                normalIndex[0]--;
                normalIndex[1]--;
                normalIndex[2]--;
                vert0.normal = normals[normalIndex[0]];
                vert1.normal = normals[normalIndex[1]];
                vert2.normal = normals[normalIndex[2]];
            }

            u32 vertexCount = vertices->count;
            // Vertex indices could be in relative (negative) form
            if( vertexIndex[0] > 0 )
            {
                // We assume if one is positive, all are, and vice-versa
                vertexIndex[0]--;
                vertexIndex[1]--;
                vertexIndex[2]--;
            }
            else
            {
                vertexIndex[0] = vertices->count - vertexIndex[0];
                vertexIndex[1] = vertices->count - vertexIndex[1];
                vertexIndex[2] = vertices->count - vertexIndex[2];
            }

            indices->Add( vertexIndex[0] );
            indices->Add( vertexIndex[1] );
            indices->Add( vertexIndex[2] );
        }
    }
}
