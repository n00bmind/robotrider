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

#version 330 core

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

in VertexData
{
    flat uint color;
} _in[];

out VertexData
{
    flat uint color;
    flat vec3 faceNormal;
} _out;


void main()
{
    vec3 v1 = (gl_in[1].gl_Position - gl_in[0].gl_Position).xyz;
    vec3 v2 = (gl_in[2].gl_Position - gl_in[0].gl_Position).xyz;
    vec3 normal = normalize( cross( v1, v2 ) );

    for( int i = 0; i < gl_in.length(); ++i )
    {
        gl_Position = gl_in[i].gl_Position;
        _out.color = _in[i].color;
        _out.faceNormal = normal;
        EmitVertex();
    }
    EndPrimitive();
}
