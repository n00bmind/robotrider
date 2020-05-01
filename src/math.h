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

inline bool
AlmostEqual( r32 a, r32 b, r32 absoluteEpsilon = 0 )
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
GreaterOrAlmostEqual( r32 a, r32 b, r32 absoluteEpsilon = 0 )
{
    return a > b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
LessOrAlmostEqual( r32 a, r32 b, r32 absoluteEpsilon = 0 )
{
    return a < b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
AlmostEqual( r64 a, r64 b, r64 absoluteEpsilon = 0 )
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

inline r32
Radians( r32 degrees )
{
    r32 result = degrees * PI / 180;
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

inline r32
RandomNormalizedR32()
{
    r32 result = (r32)rand() / (r32)RAND_MAX;
    return result;
}

inline r32
RandomBinormalizedR32()
{
    r32 result = RandomNormalizedR32() * 2.f - 1.f;
    return result;
}

inline r64
RandomNormalizedR64()
{
    r64 result = (r64)rand() / (r64)RAND_MAX;
    return result;
}

inline r64
RandomBinormalizedR64()
{
    r64 result = RandomNormalizedR64() * 2.0 - 1.0;
    return result;
}

inline u32
RandomU32()
{
    return (u32)(RandomNormalizedR32() * U32MAX);
}

inline i32
RandomI32()
{
    return (i32)(RandomNormalizedR32() * U32MAX);
}

inline u64
RandomU64()
{
    return (u64)(RandomNormalizedR64() * U64MAX);
}

inline u64
RandomI64()
{
    return (i64)(RandomNormalizedR64() * U64MAX);
}

// Includes min & max
inline i32
RandomRangeI32( i32 min, i32 max )
{
    ASSERT( min < max );
    r32 t = RandomNormalizedR32();
    i32 result = (i32)(min + t * (max - min));
    return result;
}

inline u32
RandomRangeU32( u32 min, u32 max )
{
    ASSERT( min < max );
    r32 t = RandomNormalizedR32();
    u32 result = (u32)(min + t * (max - min));
    return result;
}

inline r32
RandomRangeR32( r32 min, r32 max )
{
    ASSERT( min < max );
    r32 t = RandomNormalizedR32();
    r32 result = min + t * (max - min);
    return result;
}

inline r64
RandomRangeR64( r64 min, r64 max )
{
    ASSERT( min < max );
    r64 t = RandomNormalizedR64();
    r64 result = min + t * (max - min);
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

// TODO Substitute with Meow Hash?
inline u32
Fletcher32( const void* buffer, sz len )
{
	const u8* data = (u8*)buffer;
	u32 fletch1 = 0xFFFF;
	u32 fletch2 = 0xFFFF;

	while( data && len )
	{
		sz l = (len <= 360) ? len : 360;
		len -= l;
		while (l)
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
Pack01ToRGBA( r32 r, r32 g, r32 b, r32 a )
{
    u32 result = (((Round( a * 255 ) & 0xFF) << 24)
                | ((Round( b * 255 ) & 0xFF) << 16)
                | ((Round( g * 255 ) & 0xFF) << 8)
                |  (Round( r * 255 ) & 0xFF));
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
