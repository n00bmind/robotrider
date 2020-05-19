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


inline r32 SDFBox( v3 const& p, v3 const& hdim, r32 r = 0.f )
{
    const v3 d = Abs( p ) - hdim;
    return Length( { Max( d.x, 0.f ), Max( d.y, 0.f) , Max( d.z, 0.f ) } )
        + Min( Max( d.x, Max( d.y, d.z ) ), 0.f )
        - r;
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


#endif /* __MATH_SDF_H__ */
