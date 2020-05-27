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
AlmostEqual( r32 a, r32 b, r32 absoluteEpsilon /*= 0*/ )
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
GreaterOrAlmostEqual( r32 a, r32 b, r32 absoluteEpsilon /*= 0*/ )
{
    return a > b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
LessOrAlmostEqual( r32 a, r32 b, r32 absoluteEpsilon /*= 0*/ )
{
    return a < b || AlmostEqual( a, b, absoluteEpsilon );
}

inline bool
AlmostEqual( r64 a, r64 b, r64 absoluteEpsilon /*= 0*/ )
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

// TODO SIMD
v3 QEFMinimizePlanesProbabilistic( v3 const* points, v3 const* normals, int count, float stdDevP, float stdDevN )
{
	float A00 = 0.f;
	float A01 = 0.f;
	float A02 = 0.f;
	float A11 = 0.f;
	float A12 = 0.f;
	float A22 = 0.f;

	float b0 = 0.f;
	float b1 = 0.f;
	float b2 = 0.f;

	float c = 0.f;


	for( int i = 0; i < count; ++i )
	{
		v3 const& p = points[i];
		v3 const& n = normals[i];

		const float nx = n.x;
		const float ny = n.y;
		const float nz = n.z;
		const float nxny = nx * ny;
		const float nxnz = nx * nz;
		const float nynz = ny * nz;
        const float sn2 = stdDevN * stdDevN;

		const v3 A0 = { nx * nx + sn2, nxny, nxnz };
		const v3 A1 = { nxny, ny * ny + sn2, nynz };
		const v3 A2 = { nxnz, nynz, nz * nz + sn2 };
		A00 += A0.x;
		A01 += A0.y;
		A02 += A0.z;
		A11 += A1.y;
		A12 += A1.z;
		A22 += A2.z;

		const float pn = p.x * n.x + p.y * n.y + p.z * n.z;
		v3 const b = n * pn + p * sn2;
		b0 += b.x;
		b1 += b.y;
		b2 += b.z;

		const float sp2 = stdDevP * stdDevP;
		const float pp = p.x * p.x + p.y * p.y + p.z * p.z;
		const float nn = n.x * n.x + n.y * n.y + n.z * n.z;
		c += pn * pn + sn2 * pp + sp2 * nn + 3 * sp2 * sn2;
	}

	// Solving Ax = r with some common subexpressions precomputed
	float A00A12 = A00 * A12;
	float A01A22 = A01 * A22;
	float A11A22 = A11 * A22;
	float A02A12 = A02 * A12;
	float A02A11 = A02 * A11;

	float A01A12_A02A11 = A01 * A12 - A02A11;
	float A01A02_A00A12 = A01 * A02 - A00A12;
	float A02A12_A01A22 = A02A12 - A01A22;

	float denom = A00 * A11A22 + 2.f * A01 * A02A12 - A00A12 * A12 - A01A22 * A01 - A02A11 * A02;
	ASSERT( denom != 0.f );
    denom = 1.f / denom;
	float nom0 = b0 * (A11A22 - A12 * A12) + b1 * A02A12_A01A22 + b2 * A01A12_A02A11;
	float nom1 = b0 * A02A12_A01A22 + b1 * (A00 * A22 - A02 * A02) + b2 * A01A02_A00A12;
	float nom2 = b0 * A01A12_A02A11 + b1 * A01A02_A00A12 + b2 * (A00 * A11 - A01 * A01);

	v3 result = { nom0 * denom, nom1 * denom, nom2 * denom };
	return result;
}

#endif /* __MATH_H__ */
