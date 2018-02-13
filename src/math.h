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

#ifndef __MATH_H__
#define __MATH_H__ 

#include "intrinsics.h"
#include "math_types.h"
#define PI32 3.141592653589f
#define PI64 3.14159265358979323846

inline r32
Radians( r32 degrees )
{
    r32 result = degrees * PI32 / 180;
    return result;
}

inline u32
Pack01ToRGBA( v4 unpacked )
{
    u32 result = (((Round(unpacked.a * 255) & 0xFF) << 24)
                | ((Round(unpacked.b * 255) & 0xFF) << 16)
                | ((Round(unpacked.g * 255) & 0xFF) << 8)
                | ((Round(unpacked.r * 255) & 0xFF) << 0));
    return result;
}
#endif /* __MATH_H__ */
