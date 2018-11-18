/*The MIT License

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

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <float.h>
#include <time.h>

#include "common.h"
#include "intrinsics.h"
#include "memory.h"
#include "data_types.h"
#include "math.h"



u64 __rdtsc_start;
//#define TIME(f) ( __rdtsc_start = __rdtsc(), f, __rdtsc() - __rdtsc_start )
#define TIME(f) ( __rdtsc_start = ReadCycles(), f, ReadCycles() - __rdtsc_start )

ASSERT_HANDLER(DefaultAssertHandler)
{
    printf( "ASSERTION FAILED! :: '%s' (%s@%d)\n", msg, file, line );
}

// TODO Print error messages
#define EXPECT_TRUE(expr) ((void)( (expr) || HALT()))


template <typename T>
internal Array<T> NewArray( u32 n )
{
    Array<T> result = Array<T>( new T[n], n );
    result.count = n;
    return result;
}

template <typename T>
internal Array<T> CopyArray( const Array<T>& source )
{
    Array<T> result = source;
    result.data = new T[source.maxCount];
    COPY( source.data, result.data, source.count * sizeof(T) );
    return result;
}

template <typename T>
internal void DeleteArray( Array<T>& array )
{
    delete[] array.data;
    array.data = nullptr;
}

template <typename T>
internal T Average( const Array<T>& array )
{
    T result = 0;
    for( u32 i = 0; i < array.count; ++i )
    {
        T prevResult = result;
        result += array[i];
        // Check for overflow
        ASSERT( result >= prevResult );
    }
    result /= array.count;
    return result;
}


struct SortingBenchmark
{
    MemoryArena* tmpArena;

    Array<i32> sorted;
    Array<i32> reversed;
    Array<i32> random;
    Array<i32> duplicated;
    Array<i32> allEqual;

    struct Timings
    {
        const char* name;

        Array<u64> sortedTimes;
        Array<u64> reversedTimes;
        Array<u64> randomTimes;
        Array<u64> duplicatedTimes;
        Array<u64> allEqualTimes;
    };
};

internal SortingBenchmark
SetUpSortingBenchmark( u32 N, bool ascending, MemoryArena* tmpArena )
{
    SortingBenchmark result = {};
    result.tmpArena = tmpArena;

    {
        // Almost sorted
        result.sorted = NewArray<i32>( N );

        i32 currentValue = ascending ? I32MIN : I32MAX;
        i32 maxStep = (U32MAX / N) * (ascending ? 1 : -1);
        for( u32 i = 0; i < result.sorted.maxCount; ++i )
        {
            i32 deviation = 0;
            r32 p = RandomNormalizedR32();
            if( p < 0.25f )
                deviation = RandomI32();

            result.sorted[i] = currentValue + deviation;

            p = RandomNormalizedR32();
            if( p < 0.1f )
                continue;
            else
            {
                i32 step = RandomRangeI32( 0, maxStep );
                currentValue += step;
            }
        }
    }

    {
        // Almost totally reversed
        result.reversed = NewArray<i32>( N );
        ascending = !ascending;

        i32 currentValue = ascending ? I32MIN : I32MAX;
        i32 maxStep = (U32MAX / N) * (ascending ? 1 : -1);
        for( u32 i = 0; i < result.reversed.maxCount; ++i )
        {
            i32 deviation = 0;
            r32 p = RandomNormalizedR32();
            if( p < 0.25f )
                deviation = RandomI32();

            result.reversed[i] = currentValue + deviation;

            p = RandomNormalizedR32();
            if( p < 0.1f )
                continue;
            else
            {
                i32 step = RandomRangeI32( 0, maxStep );
                currentValue += step;
            }
        }
    }

    {
        // Random
        result.random = NewArray<i32>( N );
        for( u32 i = 0; i < result.random.maxCount; ++i )
        {
            result.random[i] = RandomI32();
        }
    }

    {
        // Few uniques
        result.duplicated = NewArray<i32>( N );
        ZERO( result.duplicated.data, N * sizeof(i32) );

        u32 duplicateCount = (u32)(N * RandomRangeR32( 0.5f, 0.75f ));
        u32 nextSwitch = RandomRangeU32( (u32)(duplicateCount * 0.1f), duplicateCount );
        i32 value = RandomI32();
        for( u32 d = 0; d < duplicateCount; ++d )
        {
            u32 i = RandomRangeU32( 0, N );
            // NOTE We should check that the position hasn't already been asigned
            result.duplicated[i] = value;

            if( d % nextSwitch == 0 )
            {
                value = RandomI32();
            }
        }
        for( u32 i = 0; i < result.duplicated.maxCount; ++i )
        {
            if( !result.duplicated[i] )
                result.duplicated[i] = RandomI32();
        }
    }

    {
        // All items equal
        result.allEqual = NewArray<i32>( N );
        i32 value = RandomI32();
        SET( result.allEqual.data, value, N * sizeof(i32) );
    }

    return result;
}

internal void
TearDownSortingBenchmark( SortingBenchmark* benchmark )
{
    DeleteArray( benchmark->sorted );
    DeleteArray( benchmark->reversed );
    DeleteArray( benchmark->random );
    DeleteArray( benchmark->duplicated );
    DeleteArray( benchmark->allEqual );

    ClearArena( benchmark->tmpArena );
}

internal SortingBenchmark::Timings
InitTimings( const char* name, u32 passes )
{
    SortingBenchmark::Timings result = {};

    result.name = name;
    result.sortedTimes = NewArray<u64>( passes );
    result.reversedTimes = NewArray<u64>( passes );
    result.randomTimes = NewArray<u64>( passes );
    result.duplicatedTimes = NewArray<u64>( passes );
    result.allEqualTimes = NewArray<u64>( passes );

    return result;
}

internal void
DeleteTimings( SortingBenchmark::Timings* timings )
{
    DeleteArray( timings->sortedTimes );
    DeleteArray( timings->reversedTimes );
    DeleteArray( timings->randomTimes );
    DeleteArray( timings->duplicatedTimes );
    DeleteArray( timings->allEqualTimes );
}

internal bool
VerifySorted( const Array<i32>& input, bool ascending )
{
    bool result = true;
    for( u32 i = 0; i < input.count - 1; ++i )
    {
        bool ordered = ascending ? input[i] <= input[i+1] : input[i] >= input[i+1];
        if( !ordered )
        {
            result = false;
            break;
        }
    }
    return result;
}

void
TestSortingBenchmark( SortingBenchmark* benchmark, u32 passes, bool ascending )
{
#if 0
    if( benchmark->sorted.count < 10000 )
    {
        SortingBenchmark::Timings insertionSort = InitTimings( "Insertion Sort", passes );
        for( u32 i = 0; i < passes; ++i )
        {
            Array<i32> sorted = CopyArray( benchmark->sorted );
            insertionSort.sortedTimes[i] = TIME( InsertSort( &sorted, ascending ) );
            EXPECT_TRUE( VerifySorted( sorted, ascending ) );
            //printf( "%s (sorted, %u) :: %llu cycles\n", insertionSort.name, benchmark->sorted.count, insertionSort.sortedTimes[i] );
        }
        printf( "%s (sorted, %u) :: %llu cycles\n", insertionSort.name, benchmark->sorted.count, Average( insertionSort.sortedTimes ) );
    }
#endif

    SortingBenchmark::Timings quickSort = InitTimings( "Quick Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        Array<i32> sorted = CopyArray( benchmark->sorted );
        quickSort.sortedTimes[i] = TIME( QuickSort( &sorted, ascending ) );
        EXPECT_TRUE( VerifySorted( sorted, ascending ) );

        Array<i32> reversed = CopyArray( benchmark->reversed );
        quickSort.reversedTimes[i] = TIME( QuickSort( &reversed, ascending ) );
        EXPECT_TRUE( VerifySorted( reversed, ascending ) );

        Array<i32> random = CopyArray( benchmark->random );
        quickSort.randomTimes[i] = TIME( QuickSort( &random, ascending ) );
        EXPECT_TRUE( VerifySorted( random, ascending ) );

        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        quickSort.duplicatedTimes[i] = TIME( QuickSort( &duplicated, ascending ) );
        EXPECT_TRUE( VerifySorted( duplicated, ascending ) );

        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        quickSort.allEqualTimes[i] = TIME( QuickSort( &allEqual, ascending ) );
        EXPECT_TRUE( VerifySorted( allEqual, ascending ) );
    }
    printf( "%s (sorted, %u)\t:: %llu cycles\n", quickSort.name, benchmark->sorted.count, Average( quickSort.sortedTimes ) );
    printf( "%s (reversed, %u)\t:: %llu cycles\n", quickSort.name, benchmark->reversed.count, Average( quickSort.reversedTimes ) );
    printf( "%s (random, %u)\t:: %llu cycles\n", quickSort.name, benchmark->random.count, Average( quickSort.randomTimes ) );
    printf( "%s (duplicated, %u)\t:: %llu cycles\n", quickSort.name, benchmark->duplicated.count, Average( quickSort.duplicatedTimes ) );
    printf( "%s (allEqual, %u)\t:: %llu cycles\n", quickSort.name, benchmark->allEqual.count, Average( quickSort.allEqualTimes ) );

    DeleteTimings( &quickSort );

    SortingBenchmark::Timings radixSort = InitTimings( "Radix Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        ClearArena( benchmark->tmpArena );
        Array<i32> sorted = CopyArray( benchmark->sorted );
        radixSort.sortedTimes[i] = TIME( RadixSort( &sorted, ascending, benchmark->tmpArena ) );
        EXPECT_TRUE( VerifySorted( sorted, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> reversed = CopyArray( benchmark->reversed );
        radixSort.reversedTimes[i] = TIME( RadixSort( &reversed, ascending, benchmark->tmpArena ) );
        EXPECT_TRUE( VerifySorted( reversed, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> random = CopyArray( benchmark->random );
        radixSort.randomTimes[i] = TIME( RadixSort( &random, ascending, benchmark->tmpArena ) );
        EXPECT_TRUE( VerifySorted( random, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        radixSort.duplicatedTimes[i] = TIME( RadixSort( &duplicated, ascending, benchmark->tmpArena ) );
        EXPECT_TRUE( VerifySorted( duplicated, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        radixSort.allEqualTimes[i] = TIME( RadixSort( &allEqual, ascending, benchmark->tmpArena ) );
        EXPECT_TRUE( VerifySorted( allEqual, ascending ) );

    }
    printf( "%s (sorted, %u)\t:: %llu cycles\n", radixSort.name, benchmark->sorted.count, Average( radixSort.sortedTimes ) );
    printf( "%s (reversed, %u)\t:: %llu cycles\n", radixSort.name, benchmark->reversed.count, Average( radixSort.reversedTimes ) );
    printf( "%s (random, %u)\t:: %llu cycles\n", radixSort.name, benchmark->random.count, Average( radixSort.randomTimes ) );
    printf( "%s (duplicated, %u)\t:: %llu cycles\n", radixSort.name, benchmark->duplicated.count, Average( radixSort.duplicatedTimes ) );
    printf( "%s (allEqual, %u)\t:: %llu cycles\n", radixSort.name, benchmark->allEqual.count, Average( radixSort.allEqualTimes ) );

    printf( "\n" );
}




void
main( int argC, char** argV )
{
    RandomSeed();

    MemoryArena tmpArena;
    u32 memorySize = 4*1024*1024;
    InitArena( &tmpArena, new u8[memorySize], memorySize );

    bool testSortingBenchmark = true;

    if( testSortingBenchmark )
    {
        SortingBenchmark sortingBenchmark = SetUpSortingBenchmark( 100, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 10000, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000000, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );
    }
}
