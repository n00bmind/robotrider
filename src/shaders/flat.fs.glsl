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

in VertexData
{
    flat uint color;
    flat vec3 faceNormal;
} _in;

out vec4 outColor;


// TODO Move this to an include when we support that
// vec4 to rgba8 uint
// TODO Test
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
    vec3 lightDirection = vec3( 0.0, 0.0, 1.0 );
    float d = dot( lightDirection, _in.faceNormal );
    //outColor = unpack( _in.color );
    outColor = vec4( d, d, d, 1 );
}
