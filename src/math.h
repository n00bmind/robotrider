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

#if NON_UNITY_BUILD
#include <float.h>
#include <time.h>
#include <stdlib.h>
#include "intrinsics.h"
#endif

#define PI   3.141592653589f
#define PI64 3.14159265358979323846

// NOTE Absolute epsilon comparison will be necessary when comparing against zero
inline bool
AlmostEqual( f32 a, f32 b, f32 absoluteEpsilon /*= 0*/ )
{
    bool result = false;

    if( absoluteEpsilon == 0 )
    {
        result = Abs( a - b ) <= Abs( Max( a, b ) ) * FLT_EPSILON;
    }
    else
    {
        result = Abs( a - b ) <= absoluteEpsilon;
    }

    return result;
}

inline bool
AlmostZero( f32 v, f32 absoluteEpsilon = 0.001f )
{
    return Abs( v ) <= absoluteEpsilon;
}

inline bool
GreaterOrAlmostEqual( f32 a, f32 b, f32 absoluteEpsilon /*= 0*/ )
{
    return a > b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
LessOrAlmostEqual( f32 a, f32 b, f32 absoluteEpsilon /*= 0*/ )
{
    return a < b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
AlmostEqual( f64 a, f64 b, f64 absoluteEpsilon /*= 0*/ )
{
    bool result = false;

    if( absoluteEpsilon == 0 )
    {
        result = Abs( a - b ) <= Abs( Max( a, b ) ) * FLT_EPSILON;
    }
    else
    {
        result = Abs( a - b ) <= absoluteEpsilon;
    }

    return result;
}

inline f32
Radians( f32 degrees )
{
    f32 result = degrees * PI / 180;
    return result;
}

// FIXME Make all random generating functions repeatable for PCG
// https://www.gamasutra.com/blogs/RuneSkovboJohansen/20150105/233505/A_Primer_on_Repeatable_Random_Numbers.php
// https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/generating-random-numbers
// http://www.pcg-random.org/
// Also, we should probably distinguish between "fast & repeatable" (for PCG) and "good distribution" (for other stuff) random numbers
// FIXME All random functions should have the seed passed in, or even better, a Distribution which already was created using one.

inline void
RandomSeed()
{
    srand( (u32)time( nullptr ) );
}

inline f32
RandomNormalizedF32()
{
    f32 result = (f32)rand() / (f32)RAND_MAX;
    return result;
}

inline f32
RandomBinormalizedF32()
{
    f32 result = RandomNormalizedF32() * 2.f - 1.f;
    return result;
}

inline f64
RandomNormalizedF64()
{
    f64 result = (f64)rand() / (f64)RAND_MAX;
    return result;
}

inline f64
RandomBinormalizedF64()
{
    f64 result = RandomNormalizedF64() * 2.0 - 1.0;
    return result;
}

inline u32
RandomU32()
{
    return (u32)(RandomNormalizedF32() * U32MAX);
}

inline i32
RandomI32()
{
    return (i32)(RandomNormalizedF32() * U32MAX);
}

inline u64
RandomU64()
{
    return (u64)(RandomNormalizedF64() * U64MAX);
}

inline i64
RandomI64()
{
    return (i64)(RandomNormalizedF64() * U64MAX);
}

// Includes min & max
inline i32
RandomRangeI32( i32 min, i32 max )
{
    ASSERT( min < max );
    f32 t = RandomNormalizedF32();
    i32 result = (i32)(min + t * (max - min));
    return result;
}

inline u32
RandomRangeU32( u32 min, u32 max )
{
    ASSERT( min < max );
    f32 t = RandomNormalizedF32();
    u32 result = (u32)(min + t * (max - min));
    return result;
}

inline f32
RandomRangeF32( f32 min, f32 max )
{
    ASSERT( min < max );
    f32 t = RandomNormalizedF32();
    f32 result = min + t * (max - min);
    return result;
}

inline f64
RandomRangeF64( f64 min, f64 max )
{
    ASSERT( min < max );
    f64 t = RandomNormalizedF64();
    f64 result = min + t * (max - min);
    return result;
}

internal int LogTable256[256] = 
{
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
#undef LT
};

inline int
Log2( u32 value )
{
    int result;
    u32 t, tt;

    tt = value >> 16;
    if( tt )
    {
        t = tt >> 8;
        result = t
            ? 24 + LogTable256[t]
            : 16 + LogTable256[tt];
    }
    else 
    {
        t = value >> 8;
        result = t
            ? 8 + LogTable256[t]
            : LogTable256[value];
    }

    return result;
}

// TODO Replace with MurmurHash3 128 (just truncate for 32/64 bit hashes) (https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp)
// (improve it with http://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html)
// TODO Add Meow Hash for hashing large blocks (also get the code and study it a little, there's gems in the open there!)
inline u32
Fletchef32( const void* buffer, int len )
{
	const u8* data = (u8*)buffer;
	u32 fletch1 = 0xFFFF;
	u32 fletch2 = 0xFFFF;

	while( data && len > 0 )
	{
		int l = (len <= 360) ? len : 360;
		len -= l;
		while( l > 0 )
		{
            fletch1 += *data++;
            fletch2 += fletch1;
            l--;
		}
		fletch1 = (fletch1 & 0xFFFF) + (fletch1 >> 16);
		fletch2 = (fletch2 & 0xFFFF) + (fletch2 >> 16);
	}
	return (fletch2 << 16) | (fletch1 & 0xFFFF);
}

inline u32
Pack01ToRGBA( f32 r, f32 g, f32 b, f32 a )
{
    u32 result = (((U32( Round( a * 255 ) ) & 0xFF) << 24)
                | ((U32( Round( b * 255 ) ) & 0xFF) << 16)
                | ((U32( Round( g * 255 ) ) & 0xFF) << 8)
                |  (U32( Round( r * 255 ) ) & 0xFF));
    return result;
}

inline u32
PackRGBA( u32 r, u32 g, u32 b, u32 a )
{
    u32 result = (((a & 0xFF) << 24)
                | ((b & 0xFF) << 16)
                | ((g & 0xFF) <<  8)
                |  (r & 0xFF));
    return result;
}

inline void
UnpackRGBA( u32 c, u32* r, u32* g, u32* b, u32* a = nullptr )
{
    if( a ) *a = (c >> 24);
    if( b ) *b = (c >> 16) & 0xFF;
    if( g ) *g = (c >>  8) & 0xFF;
    if( r ) *r = c & 0xFF;
}

#endif /* __MATH_H__ */
