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

enum class RadixTransform
{
    None = 0,
    Integer,
    FloatingPoint,
};

// NOTE May be faster than RadixSort11 for a small number of items (< 1000)
// NOTE Modifies 'in' array during sorting
void RadixSort( u32* inOut, u32 count, bool ascending, RadixTransform transform, MemoryArena* tmpArena )
{
    u32* in = inOut;
    u32* out = PUSH_ARRAY( tmpArena, count, u32 );

    const u32 RadixBits = 8;
    const u32 Radix = 1 << RadixBits;

    bool firstPass = true;
    u32 digit = 0, maxValue = 0;
    while( firstPass || maxValue > 0 )
    {
        // CountSort
        {
            const u32 Exp = digit * RadixBits;
            const u32 Mask = (Radix - 1) << Exp;

            u32 digitCounts[Radix] = {0};

            // Build digit count
            u32* it = in;
            for( u32 i = 0; i < count; ++i, ++it )
            {
                // On the first pass, also transform input and calc maximum value
                if( firstPass )
                {
                    if( transform == RadixTransform::Integer )
                        *it += 0x80000000;
                    else if( transform == RadixTransform::FloatingPoint )
                        *it ^= (-i32(*it >> 31) | 0x80000000);

                    if( *it > maxValue )
                        maxValue = *it;
                }

                u32 digitIdx = (*it & Mask) >> Exp;
                if( !ascending )
                    digitIdx = Radix - digitIdx - 1;
                digitCounts[digitIdx]++;
            }
            firstPass = false;

            u32* dc = digitCounts;
            u32 sum = 0, sumPrev = 0;
            for( u32 i = 0; i < Radix; ++i, ++dc )
            {
                if( *dc )
                {
                    sum = sumPrev + *dc;
                    *dc = sumPrev - 1u;
                    sumPrev = sum;
                }
            }

            it = in;
            for( u32 i = 0; i < count; ++i, ++it )
            {
                u32 digitIdx = (*it & Mask) >> Exp;
                if( !ascending )
                    digitIdx = Radix - digitIdx - 1;

                bool lastPass = (maxValue & ~(Radix - 1)) == 0;
                if( lastPass )
                {
                    if( transform == RadixTransform::Integer )
                        *it -= 0x80000000;
                    else if( transform == RadixTransform::FloatingPoint )
                        *it ^= (((*it >> 31) - 1) | 0x80000000);
                }
                out[++(digitCounts[digitIdx])] = *it;
            }
        }

        maxValue = maxValue >> RadixBits;
        digit++;

        u32* swap = in;
        in = out;
        out = swap;
    }

    if( in != inOut )
        COPY( in, inOut, count * sizeof(u32) );
}

void
RadixSort( Array<i32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( (u32*)inputOutput->data, inputOutput->count, ascending, RadixTransform::Integer, tmpArena );
}

void
RadixSort( Array<r32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort( (u32*)inputOutput->data, inputOutput->count, ascending, RadixTransform::FloatingPoint, tmpArena );
}

// Adapted from http://stereopsis.com/radix.html
// Builds the histograms in just one pass and processes input 11 bits at a time
// so that each histogram array fits in one cache line
// NOTE Modifies 'in' array during sorting
void RadixSort11( u32* inOut, u32 count, bool ascending, RadixTransform transform, MemoryArena* tmpArena )
{
    u32* in = inOut;
    u32* out = PUSH_ARRAY( tmpArena, count, u32 );

#define MASK0(x) ( ascending ? (x & 0x7FF)            : (~x & 0x7FF) )
#define MASK1(x) ( ascending ? ((x >> 11) & 0x7FF)    : (~(x >> 11) & 0x7FF) )
#define MASK2(x) ( ascending ? (x >> 22)              : (~(x >> 22) & 0x7FF) )

    const u32 kHistSize = 2048;
    u32 hist0[kHistSize * 3] = {0};
    u32* hist1 = hist0 + kHistSize;
    u32* hist2 = hist1 + kHistSize;

    // Build counts
    u32* it = in;
    for( u32 i = 0; i < count; ++i, ++it )
    {
        // Transform input value
        if( transform == RadixTransform::Integer )
            *it += 0x80000000u;
        else if( transform == RadixTransform::FloatingPoint )
            *it ^= (-i32(*it >> 31) | 0x80000000);

        hist0[MASK0(*it)]++;
        hist1[MASK1(*it)]++;
        hist2[MASK2(*it)]++;
    }

    // Each histogram entry records the number of values with index less than itself
    u32 *it0 = hist0, *it1 = hist1, *it2 = hist2;
    u32 sum0 = 0, sum1 = 0, sum2 = 0, sum;
    for( u32 i = 0; i < kHistSize; ++i, ++it0, ++it1, ++it2 )
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
    for( u32 i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK0(*it);
        out[++hist0[idx]] = *it;
    }

    // Offset 1: Read/write histogram, copy to in
    it = out;
    for( u32 i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK1(*it);
        in[++hist1[idx]] = *it;
    }

    // Offset 2: Read/write histogram, undo transform, write to out
    it = in;
    for( u32 i = 0; i < count; ++i, ++it )
    {
        u32 idx = MASK2(*it);

        // Undo input transformation
        if( transform == RadixTransform::Integer )
            *it -= 0x80000000u;
        else if( transform == RadixTransform::FloatingPoint )
            *it ^= (((*it >> 31) - 1) | 0x80000000);

        out[++hist2[idx]] = *it;
    }

#undef MASK0
#undef MASK1
#undef MASK2

    COPY( out, inOut, count * sizeof(u32) );
}

void
RadixSort11( Array<i32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort11( (u32*)inputOutput->data, inputOutput->count, ascending, RadixTransform::Integer, tmpArena );
}

void
RadixSort11( Array<r32>* inputOutput, bool ascending, MemoryArena* tmpArena )
{
    RadixSort11( (u32*)inputOutput->data, inputOutput->count, ascending, RadixTransform::FloatingPoint, tmpArena );
}

