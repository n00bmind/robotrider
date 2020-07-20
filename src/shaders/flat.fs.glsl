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
    smooth vec4 color;
#if GS_STAGE
    vec2 barycentricP;
#endif
} _in;

out vec4 outColor;


void main()
{
    // Our 'sun' at -Z inf.
    //vec3 sunLightDirection = vec3( 0.0, 0.0, -1.0 );
    vec3 sunLightDirection = vec3( 0.0, 0.0, 1.0 );

    float wireMul = 1;
#if WIREFRAME
    vec3 barys;
    barys.xy = _in.barycentricP;
    barys.z = 1 - barys.x - barys.y;

    vec3 deltas = fwidth(barys);
	vec3 smoothing = deltas * 1;
	vec3 thickness = deltas * 0.0000001;
	barys = smoothstep( thickness, thickness + smoothing, barys );
    wireMul = min(barys.x, min(barys.y, barys.z));
    wireMul = clamp( wireMul, 0.2, 1 );

    //vec4 diffuseLight = vec4( _in.color.xyz * wireMul, 1 );
#endif

    // Face normal using derivatives of world position
    vec3 faceNormal = normalize( cross( dFdx( _in.worldP ), dFdy( _in.worldP ) ) );

    float d = clamp( dot( sunLightDirection, faceNormal ), 0, 1 );
    vec4 diffuseLight = _in.color * d;
    diffuseLight.a = _in.color.a;

    vec4 ambientLight = vec4( 0.7, 0.7, 0.7, 1 );
    diffuseLight += ambientLight;

#if 0 // Extremely simple fog
    float dMax = 400;
    float dMin = 10;
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float tFog = clamp( (dMax - dist) / (dMax - dMin), 0, 1 );

    outColor = mix( vec4( 0.95, 0.95, 0.95, 1 ), diffuseLight, tFog );
#else
    // NOTE Apparently gl_FrontFacing doesn't have good support
    outColor = vec4( (gl_FrontFacing ? diffuseLight.xyz : _in.color.xyz * 0.5) * wireMul, 1 );
#endif
}

