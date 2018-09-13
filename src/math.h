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

inline u32
Random()
{
    return rand();
}

// Includes min & max
inline i32
RandomRange( i32 min, i32 max )
{
    i32 result = min + rand() / (RAND_MAX / (max - min + 1) + 1);
    return result;
}

inline r32
RandomRange( r32 min, r32 max )
{
    r32 t = rand() / ((r32)RAND_MAX + 1);
    r32 result = min + t * (max - min);
    return result;
}

inline bool
IsPowerOf2( u64 value )
{
    return (value & (value - 1)) == 0;
}

inline sz
Align( sz size, sz alignment )
{
    ASSERT( IsPowerOf2( alignment ) );
    sz result = (size + (alignment - 1)) & ~(alignment - 1);
    return result;
}

inline void*
Align( const void* address, sz alignment )
{
    ASSERT( IsPowerOf2( alignment ) );
    void* result = (void*)(((sz)address + (alignment - 1)) & ~(alignment - 1));
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

#endif /* __MATH_H__ */
