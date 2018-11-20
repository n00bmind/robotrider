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
// Also, we should probably distinguish between "fast & repeatable" (for PCG) and "good distribution" (for other stuff) random numbers

inline void
RandomSeed()
{
    srand( (u32)time( nullptr ) );
}

inline r32
RandomNormalizedR32()
{
    r32 result = (r32)rand() / ((r32)RAND_MAX + 1.f);
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
    r64 result = (r64)rand() / ((r64)RAND_MAX + 1.);
    return result;
}

inline r64
RandomBinormalizedR64()
{
    r64 result = RandomNormalizedR64() * 2.0 - 1.0;
    return result;
}

inline i32
RandomI32()
{
    return (i32)(RandomNormalizedR32() * U32MAX);
}

inline u32
RandomU32()
{
    return (u32)(RandomNormalizedR32() * U32MAX);
}

// Includes min & max
inline i32
RandomRangeI32( i32 min, i32 max )
{
    i32 result = min + rand() / (RAND_MAX / (max - min + 1) + 1);
    return result;
}

inline u32
RandomRangeU32( u32 min, u32 max )
{
    u32 result = min + rand() / (RAND_MAX / max + 1);
    return result;
}

inline r32
RandomRangeR32( r32 min, r32 max )
{
    r32 t = RandomNormalizedR32();
    r32 result = min + t * (max - min);
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

void
InsertSort( Array<i32>* input, bool ascending, int lo = 0, int hi = -1 )
{
    i32* data = input->data;
    if( hi == -1 )
        hi = input->count - 1;

    for( int i = lo + 1; i <= hi; ++i )
    {
        int key = data[i];

        for( int j = i; j >= lo; --j )
        {
            bool ordered = ascending ? data[j-1] <= key : key <= data[j-1];
            if( j == lo || ordered )
            {
                data[j] = key;
                break;
            }
            else
                data[j] = data[j-1];
        }
    }
}

// TODO Look into how to optimize all that follow to take advantage of the first copy into an output array?

// NOTE Assumes pivot is the last element
/*internal*/ bool
Partition( Array<i32>* input, bool ascending, int lo, int hi, int pivot, int* pivotIndex )
{
    i32* data = input->data;

    int i = lo;
    bool allEqual = true;

    for( int j = lo; j < hi; ++j )
    {
        bool swap = ascending ? (data[j] <= pivot) : (data[j] >= pivot);
        if( swap )
        {
            if( i < j )
                Swap( &data[i], &data[j] );
            ++i;
            
            allEqual = allEqual && data[j] == pivot;
        }
    }
    if( i != hi )
    {
        Swap( &data[hi], &data[i] );
        allEqual = false;
    }

    *pivotIndex = i;

    bool result = i != lo && !allEqual;
    return result;
}

void
QuickSort( Array<i32>* input, bool ascending, int lo = 0, int hi = -1 )
{
    i32* data = input->data;
    if( hi == -1 )
        hi = input->count - 1;

    int n = hi - lo + 1;
    if( n <= 10 )
        InsertSort( input, ascending, lo, hi );
    else
    {
        int med = (lo + hi) / 2;
        i32 pivot = Median( data[lo], data[med], data[hi] );
        if( pivot == data[lo] )
            Swap( &data[lo], &data[hi] );
        else if( pivot == data[med] )
            Swap( &data[med], &data[hi] );

        int p;
        if( Partition( input, ascending, lo, hi, pivot, &p ) )
        {
            QuickSort( input, ascending, lo, p - 1 );
            QuickSort( input, ascending, p + 1, hi );
        }
    }
}

/*internal*/ void
CountSort( u32* in, const u32 N, u32 digit, bool ascending, u32* out )
{
    // TODO Test different bases
    const u32 RadixBits = 4;
    const u32 Radix = 1 << RadixBits;
    const u32 Exp = digit * RadixBits;
    const u32 Mask = (Radix - 1) << Exp;

    u32 digitCounts[Radix] = {0};

    for( u32 i = 0; i < N; ++i )
    {
        u32 digitIdx = (in[i] & Mask) >> Exp;
        if( !ascending )
            digitIdx = Radix - digitIdx - 1;
        digitCounts[digitIdx]++;
    }

    for( u32 i = 1; i < Radix; ++i )
        digitCounts[i] += digitCounts[i-1];

    for( int i = N - 1; i >= 0; --i )
    {
        u32 digitIdx = (in[i] & Mask) >> Exp;
        if( !ascending )
            digitIdx = Radix - digitIdx - 1;
        out[--(digitCounts[digitIdx])] = in[i];
    }

    // TODO Look into swapping in & out arrays to eliminate copies!
    for( u32 i = 0; i < N; ++i )
        in[i] = out[i];
}

// TODO Float: flip everybit if negative, flip only the sign bit if positive
void
RadixSort( Array<i32>* input, bool ascending, MemoryArena* tmpArena )
{
    const u32 RadixBits = 4;
    const u32 Radix = 1 << RadixBits;

    Array<u32> tmp = Array<u32>( tmpArena, input->count );
    tmp.count = input->count;

    u32* in = (u32*)input->data;

#pragma warning(push)
#pragma warning(disable : 4307)
#define U32WRAP (u32)(I32MAX + 1)
    // For signed ints: add 32768
    u32 maxValue = 0;
    for( u32 i = 0; i < input->count; ++i )
    {
        in[i] += U32WRAP;
        if( in[i] > maxValue )
            maxValue = in[i];
    }

    u32 digit = 0;
    while( maxValue > 0 )
    {
        CountSort( in, input->count, digit, ascending, tmp.data );
        maxValue = maxValue >> RadixBits;
        ++digit;
    }

    // Undo pre transformation
    // TODO Do this as part of the last copy from tmp to in
    for( u32 i = 0; i < input->count; ++i )
        in[i] -= U32WRAP;
#undef U32WRAP
#pragma warning(pop)
}

enum class RadixTransform
{
    None = 0,
    Integer,
    FloatingPoint,
};

// For 32 bits
// TODO Descending
// TODO Transforms
void RadixSort11( u32* in, u32 count, RadixTransform transform, u32* out )
{
#define MASK0(x) (x & 0x7FF)
#define MASK1(x) ((x >> 11) & 0x7FF)
#define MASK2(x) (x >> 22)

    const u32 kHistSize = 2048;
    u32 hist0[kHistSize * 3] = {0};
    u32* hist1 = hist0 + kHistSize;
    u32* hist2 = hist1 + kHistSize;

    // Build counts
    for( u32 i = 0; i < count; ++i )
    {
        // TODO Prefetch pf(array)

        u32 it = in[i];
        // Transform input value
        if( transform == RadixTransform::Integer )
            it += 0x80000000u;

        hist0[MASK0(it)]++;
        hist1[MASK1(it)]++;
        hist2[MASK2(it)]++;
    }

    // TODO Watch this live
    // Each histogram entry records the number of values preceding itself
    u32 totalSum, sum0 = 0, sum1 = 0, sum2 = 0;
    for( u32 i = 0; i < kHistSize; ++i )
    {
        totalSum = hist0[i] + sum0;
        hist0[i] = sum0 - 1u;
        sum0 = totalSum;

        totalSum = hist1[i] + sum1;
        hist1[i] = sum1 - 1u;
        sum1 = totalSum;

        totalSum = hist2[i] + sum2;
        hist2[i] = sum2 - 1u;
        sum2 = totalSum;
    }

    // Offset 0: Transform entire input value, read/write histogram, write to out
    for( u32 i = 0; i < count; ++i )
    {
        u32 it = in[i];
        // Transform input value
        if( transform == RadixTransform::Integer )
            it += 0x80000000u;

        u32 idx = MASK0(it);
        // TODO Prefetch pf2(array)
        out[++hist0[idx]] = it;
    }

    // Offset 1: Read/write histogram, copy to in
    for( u32 i = 0; i < count; ++i )
    {
        u32 it = out[i];
        u32 idx = MASK1(it);
        // TODO Prefetch pf2(sort)
        in[++hist1[idx]] = it;
    }

    // Offset 2: Read/write histogram, undo transform, write to out
    for( u32 i = 0; i < count; ++i )
    {
        u32 it = in[i];
        u32 idx = MASK2(it);
        // TODO Prefetch pf2(array)
        // Undo input transformation
        if( transform == RadixTransform::Integer )
            it -= 0x80000000u;

        out[++hist2[idx]] = it;
    }
#undef MASK0
#undef MASK1
#undef MASK2
}

void
RadixSort11( Array<i32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    Array<i32> tmpOut = inputOutput->Copy( tmpArena );
    RadixSort11( (u32*)inputOutput->data, inputOutput->count, RadixTransform::Integer, (u32*)tmpOut.data );
    tmpOut.CopyTo( inputOutput );
}

#endif /* __MATH_H__ */
