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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoords;
layout(location = 2) in uint inColor;

out VertexData
{
    vec3 worldP;
    vec2 texCoords;
    smooth vec4 color;
} _out;

// TODO Consider using interface block for uniforms in the future too
uniform mat4 mTransform;
// FIXME Add a #define for the size of this array from code and guard against the actual size of the source data and the OpenGL limits
// NOTE Consider using Texture Buffer Objects for more complex per-mesh data
// https://www.khronos.org/opengl/wiki/Buffer_Texture
// https://gist.github.com/roxlu/5090067
uniform vec3[256] simClusterOffsets;
uniform uint simClusterIndex;


// TODO Move these to an include when we support that
//
// TODO Test
// vec4 to rgba8 uint
uint pack( vec4 value )
{
    // Ensure values are in [0..1] and make NaNs become zeros
    value = min( max( value, 0.0f ), 1.0f );
    
    // Each component gets 8 bit
    value = value * 255 + 0.5f;
    value = floor( value );
    
    // Pack into one 32 bit uint
    return( uint(value.x) |
           (uint(value.y)<< 8) |
           (uint(value.z)<<16) |
           (uint(value.w)<<24) );
}

// rgba8 uint to vec4
vec4 unpack( uint value )
{
    return vec4(float(value & 0xFFu) / 255,
                float((value >>  8) & 0xFFu) / 255,
                float((value >> 16) & 0xFFu) / 255,
                float((value >> 24) & 0xFFu) / 255);
}


void main()
{
    _out.worldP = inPosition + simClusterOffsets[simClusterIndex];
    gl_Position = mTransform * vec4( _out.worldP, 1.0 );

    _out.texCoords = inTexCoords;
    _out.color = unpack( inColor );
}

