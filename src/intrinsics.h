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

#ifndef __INTRINSICS_H__
#define __INTRINSICS_H__ 

#if _MSC_VER
#include "intrin.h"
#endif


// TODO Convert all of these to the most platform-efficient versions
// for all supported compilers & platforms

inline u32
SafeTruncToU32( u64 value )
{
    ASSERT( value <= 0xFFFFFFFF );
    u32 result = (u32)value;
    return result;
}

inline u32
Ceil( r32 value )
{
    u32 result = (u32)(value + 0.5f);
    return result;
}

inline u32
Ceil( r64 value )
{
    u32 result = (u32)(value + 0.5);
    return result;
}

inline u32
Round( r32 value )
{
    u32 result = (u32)roundf( value );
    return result;
}

inline u32
Round( r64 value )
{
    u32 result = (u32)round( value );
    return result;
}

inline r32
Sin( r32 angleRads )
{
    r32 result = sinf( angleRads );
    return result;
}

inline r32
Cos( r32 angleRads )
{
    r32 result = cosf( angleRads );
    return result;
}

inline r32
Sqrt( r32 value )
{
    r32 result = sqrtf( value );
    return result;
}

inline r32
Min( r32 a, r32 b )
{
    return a < b ? a : b;
}

inline r64
Min( r64 a, r64 b )
{
    return a < b ? a : b;
}

inline r32
Max( r32 a, r32 b )
{
    return a > b ? a : b;
}

inline r32
Clamp0( r32 value )
{
    return Max( 0, value );
}

inline r64
Pow( r64 b, r64 exp )
{
    return pow( b, exp );
}

inline r32
Abs( r32 value )
{
    return value >= 0.f ? value : -value;
}

inline u32
GetNextPowerOf2( u32 value )
{
    u32 result = 0;

    unsigned long msbPosition;
    if( _BitScanReverse( &msbPosition, value ) )
    {
        result = 1 << (msbPosition + 1);
    }

    return result;
}

#endif /* __INTRINSICS_H__ */
