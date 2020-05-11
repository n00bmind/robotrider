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

#define GS_STAGE 1
#if GS_STAGE
#define WIREFRAME 1
#endif

in VertexData
{
    vec3 worldP;
    vec2 texCoords;
    flat uint color;
#if GS_STAGE
    vec2 barycentricP;
#endif
} _in;

out vec4 outColor;


// TODO Move this to an include when we support that
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
    vec4 vertexColor = unpack( _in.color );

#if WIREFRAME
    vec3 barys;
    barys.xy = _in.barycentricP;
    barys.z = 1 - barys.x - barys.y;

    vec3 deltas = fwidth(barys);
	vec3 smoothing = deltas * 1;
	vec3 thickness = deltas * 0.1;
	barys = smoothstep( thickness, thickness + smoothing, barys );
    float minBary = min(barys.x, min(barys.y, barys.z));

    vec4 diffuseLight = vec4( vertexColor.xyz * minBary, 1 );
#else
    // Face normal using derivatives of world position
     vec3 faceNormal = normalize( cross( dFdx( _in.worldP ), dFdy( _in.worldP ) ) );

    // Our 'sun' at -Z inf.
    vec3 lightDirection = vec3( 0.0, 0.0, -1.0 );
    // FIXME This is relative to the view,
    //float d = dot( lightDirection, _in.faceNormal );
    float d = dot( lightDirection, faceNormal );
    vec4 diffuseLight = vertexColor * d;
    diffuseLight.a = vertexColor.a;

    vec4 ambientLight = vec4( 0.5, 0.5, 0.5, 1 );
    diffuseLight += ambientLight;
#endif

#if 0 // Extremely simple fog
    float dMax = 400;
    float dMin = 10;
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float tFog = clamp( (dMax - dist) / (dMax - dMin), 0, 1 );

    outColor = mix( vec4( 0.95, 0.95, 0.95, 1 ), diffuseLight, tFog );
#else
    outColor = diffuseLight;
#endif

}
