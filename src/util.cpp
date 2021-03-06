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

#if NON_UNITY_BUILD
#include "common.h"
#include "data_types.h"
#include "util.h"
#endif


inline void
Swap( i32* a, i32* b )
{
    i32 tmp = *a;
    *a = *b;
    *b = tmp;
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

// NOTE This is here for 'curiosity' purposes. Quite unstable, use RadixSort!
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

// NOTE May be faster than RadixSort11 for a small number of items (< 1000)
// NOTE Modifies input array during sorting
// TODO This has never been analyzed with VTune or similar, so there's probably still a good chunk of perf to squeeze from it
void RadixSort( void* inOut, int count, sz offset, sz stride, RadixKey keyType, bool ascending, MemoryArena* tmpArena )
{
    u8* in = (u8*)inOut;
    u8* out = PUSH_ARRAY( tmpArena, u8, count * stride, Temporary() );

    const u32 RadixBits = 8;
    const u64 Radix = 1 << RadixBits;

    bool firstPass = true;
    bool is32Bits = keyType < RadixKey::U64;
    u32 digit = 0;
    u64 maxKey = 0;
    while( firstPass || maxKey > 0 )
    {
        // CountSort
        {
            const u64 Exp = digit * RadixBits;
            const u64 Mask = (Radix - 1) << Exp;

            u32 digitCounts[Radix] = {0};

            // Build digit count
            u64* inKey = (u64*)(in + offset);
            for( int i = 0; i < count; ++i )
            {
                u64 key = *inKey;
                if( is32Bits )
                    key &= 0xFFFFFFFFu;

                // On the first pass, also transform input and calc maximum value
                if( firstPass )
                {
                    if( keyType == RadixKey::I32 )
                        key |= 0x80000000u;
                    else if( keyType == RadixKey::F32 )
                        key ^= !!(key & 0x80000000u) ? 0xFFFFFFFFu : 0x80000000u;
                    else if( keyType == RadixKey::I64 )
                        key |= 0x8000000000000000u;
                    else if( keyType == RadixKey::F64 )
                        key ^= !!(key & 0x8000000000000000u) ? 0xFFFFFFFFFFFFFFFFu : 0x8000000000000000u;

                    if( key > maxKey )
                        maxKey = key;

                    *inKey = is32Bits ? (*inKey & 0xFFFFFFFF00000000u | key) : key; 
                }

                u64 digitIdx = (key & Mask) >> Exp;
                if( !ascending )
                    digitIdx = Radix - digitIdx - 1;
                digitCounts[digitIdx]++;

                inKey = (u64*)((u8*)inKey + stride);
            }
            firstPass = false;

            u32* dc = digitCounts;
            u32 sum = 0, sumPrev = 0;
            for( int i = 0; i < Radix; ++i, ++dc )
            {
                if( *dc )
                {
                    sum = sumPrev + *dc;
                    *dc = sumPrev - 1u;
                    sumPrev = sum;
                }
            }

            u8* inIt = in;
            for( int i = 0; i < count; ++i, inIt += stride )
            {
                inKey = (u64*)(inIt + offset);
                u64 key = *inKey;
                if( is32Bits )
                    key &= 0xFFFFFFFFu;

                u64 digitIdx = (key & Mask) >> Exp;
                if( !ascending )
                    digitIdx = Radix - digitIdx - 1;

                bool lastPass = maxKey < Radix;
                if( lastPass )
                {
                    if( keyType == RadixKey::I32 )
                        key |= 0x80000000u;
                    else if( keyType == RadixKey::F32 )
                        key ^= !!(key & 0x80000000u) ? 0x80000000u : 0xFFFFFFFFu;
                    else if( keyType == RadixKey::I64 )
                        key |= 0x8000000000000000u;
                    else if( keyType == RadixKey::F64 )
                        key ^= !!(key & 0x8000000000000000u) ? 0x8000000000000000u : 0xFFFFFFFFFFFFFFFFu;

                    *inKey = is32Bits ? (*inKey & 0xFFFFFFFF00000000u | key) : key; 
                }

                u32 outIdx = ++(digitCounts[digitIdx]);
                u8* outIt = out + outIdx * stride;
                PCOPY( inIt, outIt, stride );
            }
        }

        maxKey = maxKey >> RadixBits;
        digit++;

        u8* swap = in;
        in = out;
        out = swap;
    }

    if( in != inOut )
        PCOPY( in, inOut, count * stride );
}

void
RadixSort( Array<u32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u32), RadixKey::U32, ascending, tmpArena );
}

void
RadixSort( Array<i32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u32), RadixKey::I32, ascending, tmpArena );
}

void
RadixSort( Array<f32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u32), RadixKey::F32, ascending, tmpArena );
}

void
RadixSort( Array<KeyIndex>* inputOutput, RadixKey keyType, bool ascending, MemoryArena* tmpArena )
{
    ASSERT( keyType < RadixKey::U64 );
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(KeyIndex), keyType, ascending, tmpArena );
}

void
RadixSort( Array<u64>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u64), RadixKey::U64, ascending, tmpArena );
}

void
RadixSort( Array<i64>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u64), RadixKey::I64, ascending, tmpArena );
}

void
RadixSort( Array<f64>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(u64), RadixKey::F64, ascending, tmpArena );
}

void RadixSort( Array<KeyIndex64>* inputOutput, RadixKey keyType, bool ascending, MemoryArena* tmpArena )
{
    ASSERT( keyType >= RadixKey::U64 );
    RadixSort( inputOutput->data, inputOutput->count, 0, sizeof(KeyIndex64), keyType, ascending, tmpArena );
}

template <typename T> void
RadixSort( Array<T>* inputOutput, sz offset, RadixKey keyType, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( inputOutput->data, inputOutput->count, offset, sizeof(T), keyType, ascending, tmpArena );
}

// Adapted from http://stereopsis.com/radix.html
// Builds the histograms in just one pass and processes input 11 bits at a time
// so that each histogram array fits in one cache line
// NOTE Modifies 'in' array during sorting
void RadixSort11( u32* inOut, int count, bool ascending, RadixKey transform, MemoryArena* tmpArena )
{
    u32* in = inOut;
    u32* out = PUSH_ARRAY( tmpArena, u32, count, Temporary() );

#define MASK0(x) ( ascending ? (x & 0x7FF)            : (~x & 0x7FF) )
#define MASK1(x) ( ascending ? ((x >> 11) & 0x7FF)    : (~(x >> 11) & 0x7FF) )
#define MASK2(x) ( ascending ? (x >> 22)              : (~(x >> 22) & 0x7FF) )

    const u32 kHistSize = 2048;
    u32 hist0[kHistSize * 3] = {0};
    u32* hist1 = hist0 + kHistSize;
    u32* hist2 = hist1 + kHistSize;

    // Build counts
    u32* it = in;
    for( int i = 0; i < count; ++i, ++it )
    {
        // Transform input value
        if( transform == RadixKey::I32 )
            *it += 0x80000000u;
        else if( transform == RadixKey::F32 )
            *it ^= (-i32(*it >> 31) | 0x80000000);

        hist0[MASK0(*it)]++;
        hist1[MASK1(*it)]++;
        hist2[MASK2(*it)]++;
    }

    // Each histogram entry records the number of values with index less than itself
    u32 *it0 = hist0, *it1 = hist1, *it2 = hist2;
    u32 sum0 = 0, sum1 = 0, sum2 = 0, sum;
    for( int i = 0; i < kHistSize; ++i, ++it0, ++it1, ++it2 )
    {
        if( *it0 )
        {
            sum = sum0 + *it0;
            *it0 = sum0 - 1u;
            sum0 = sum;
        }

        if( *it1 )
        {
            sum = sum1 + *it1;
            *it1 = sum1 - 1u;
            sum1 = sum;
        }

        if( *it2 )
        {
            sum = sum2 + *it2;
            *it2 = sum2 - 1u;
            sum2 = sum;
        }
    }

    // Offset 0: Transform entire input value, read/write histogram, write to out
    it = in;
    for( int i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK0(*it);
        out[++hist0[idx]] = *it;
    }

    // Offset 1: Read/write histogram, copy to in
    it = out;
    for( int i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK1(*it);
        in[++hist1[idx]] = *it;
    }

    // Offset 2: Read/write histogram, undo transform, write to out
    it = in;
    for( int i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK2(*it);

        // Undo input transformation
        if( transform == RadixKey::I32 )
            *it -= 0x80000000u;
        else if( transform == RadixKey::F32 )
            *it ^= (((*it >> 31) - 1) | 0x80000000);

        out[++hist2[idx]] = *it;
    }

#undef MASK0
#undef MASK1
#undef MASK2

    PCOPY( out, inOut, count * sizeof(u32) );
}

void
RadixSort11( Array<i32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort11( (u32*)inputOutput->data, inputOutput->count, ascending, RadixKey::I32, tmpArena );
}

void
RadixSort11( Array<f32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort11( (u32*)inputOutput->data, inputOutput->count, ascending, RadixKey::F32, tmpArena );
}


template <typename T> void
BuildSortableKeysArray( const Array<T>& sourceTypeArray, sz typeKeyOffset, Array<KeyIndex>* result )
{
    for( int i = 0; i < sourceTypeArray.count; ++i )
    {
        u8* base = (u8*)&sourceTypeArray[i];
        u32* key = (u32*)(base + typeKeyOffset);
        result->Push( { *key, i } );
    }
}

template <typename T> void
BuildSortableKeysArray( const Array<T>& sourceTypeArray, sz typeKeyOffset, Array<KeyIndex64>* result )
{
    for( int i = 0; i < sourceTypeArray.count; ++i )
    {
        u8* base = (u8*)&sourceTypeArray[i];
        u64* key = (u64*)(base + typeKeyOffset);
        result->Push( { *key, i } );
    }
}

