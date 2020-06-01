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

#ifndef __MATH_SDF_H__
#define __MATH_SDF_H__ 


inline r32 SDFUnion( r32 d1, r32 d2 )
{
    return Min( d1, d2 );
}

inline r32 SDFIntersection( r32 d1, r32 d2 )
{
    return Max( d1, d2 );
}

inline r32 SDFSubstraction( r32 d1, r32 d2 )
{
    return Max( d1, -d2  );
}


inline r32 SDFBox( v3 const& p, v3 const& hdim, r32 r = 0.f )
{
    const v3 d = Abs( p ) - hdim;
    return Length( { Max( d.x, 0.f ), Max( d.y, 0.f) , Max( d.z, 0.f ) } )
        + Min( Max( d.x, Max( d.y, d.z ) ), 0.f )
        - r;
}

inline r32 SDFCylinder( v3 const& p, r32 r )
{
    return p.y * p.y + p.z * p.z - r * r;
}

inline r32 SDFTorus( v3 const& p, r32 r, r32 t )
{
    const v2 q = V2( Length( p.xy ) - r, p.z );
    return Length( q ) - t;
}

inline r32 SDFSphere( v3 const& p, r32 r )
{
    return Length( p ) - r;
}

inline r32 SDFHollowCube( v3 const& p )
{
    r32 pp = 15000.f;
    r32 r4 = 100000000.f;

    r32 x2 = p.x * p.x;
    r32 y2 = p.y * p.y;
    r32 z2 = p.z * p.z;

    return Sqr( x2 + y2 - pp ) + Sqr( x2 + z2 - pp ) + Sqr( z2 + y2 - pp ) - r4;
}

inline r32 SDFDevil( v3 const& p )
{
    r32 a = 15.f;
    r32 b = 3600.f;
    r32 c = 2500.f;

    r32 x2 = p.x * p.x;
    r32 y2 = p.y * p.y;
    r32 z2 = p.z * p.z;

    return x2 * x2 + a * x2 * z2 - b * x2 - y2 * y2 + c * y2 + z2 * z2;
}

inline r32 SDFQuarticCylinder( v3 const& p )
{
    r32 a = 10.f;
    r32 b = 10.f;
    r32 c = 200000.f;

    r32 x2 = p.x * p.x;
    r32 y2 = p.y * p.y;
    r32 z2 = p.z * p.z;

    return y2 * x2 + y2 * z2 + a * x2 + b * z2 - c;
}

inline r32 SDFTangleCube( v3 const& p )
{
    r32 a = 5000.f;
    r32 b = 12000000.f;

    r32 x2 = p.x * p.x;
    r32 y2 = p.y * p.y;
    r32 z2 = p.z * p.z;

    return x2 * x2 - a * x2 + y2 * y2 - a * y2 + z2 * z2 - a * z2 + b;
}

inline r32 SDFGenus2( v3 const& p )
{
    r32 a = 200.f;
    r32 b = 30.f;
    r32 c = 900.f;
    r32 d = 1000.f;
    r32 e = 10.f;

    r32 x2 = p.x * p.x;
    r32 y2 = p.y * p.y;
    r32 z2 = p.z * p.z;

    return a * p.y * (y2 - b * x2) * (d - z2) + Sqr( x2 + y2 ) - (c * z2 - e) * (d - z2);
}

inline r32 SDFTrefoilKnot( v3 const& p )
{
    r32 a = 40.f;
    r32 b = 50.f;
    r32 a2 = a * a;
    r32 b2 = b * b;

    r32 x = p.x;
    r32 y = p.y;
    r32 z = p.z;
    r32 x2 = x * x;
    r32 y2 = y * y;
    r32 z2 = z * z;
    r32 x3 = x2 * x;
    r32 y3 = y2 * y;
    r32 z3 = z2 * z;

    return Sqr(-8.f * Sqr(x2 + y2) * (x2 + y2 + 1.f + z2 + a2 - b2)
               + 4.f * a2 * (2.f * Sqr(x2 + y2) - (x3 - 3.f * x * y2) * (x2 + y2 + 1.f))
               + 8.f * a2 * (3.f * x2 * y - y3) * z + 4.f * a2 * (x3 - 3.f * x * y2) * z2)
        - (x2 + y2) * Sqr(2.f * (x2 + y2) * Sqr(x2 + y2 + 1.f + z2 + a2 - b2) + 8.f * Sqr(x2 + y2)
                          + 4.f * a2 * (2.f * (x3 - 3.f * x * y2) -(x2 + y2) * (x2 + y2 + 1.f))
                          - 8.f * a2 * (3.f * x2 * y - y3) * z - 4.f * (x2 + y2) * a2 * z2);
}


#endif /* __MATH_SDF_H__ */
