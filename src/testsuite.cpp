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

#include <windows.h>

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


internal r64 globalCounterFreq = 0.0;
internal i64 globalCounterStart;

u64 __rdtsc_start;
#define TIME(f) ( __rdtsc_start = ReadCycles(), f, ReadCycles() - __rdtsc_start )

ASSERT_HANDLER(DefaultAssertHandler)
{
    printf( "ASSERTION FAILED! :: '%s' (%s@%d)\n", msg, file, line );
}

// TODO Print error messages
#define EXPECT_TRUE(expr) ((void)( (expr) || HALT()))
#define ASSERT_TRUE(expr) ((void)( (expr) || HALT()))


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

inline void
StartCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    globalCounterStart = li.QuadPart;
}

inline r64
GetCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    return r64(li.QuadPart - globalCounterStart) / globalCounterFreq;
}



struct SortingBenchmark
{
    MemoryArena* tmpArena;
    bool ascending;

    Array<i32> sorted;
    Array<i32> reversed;
    Array<i32> random;
    Array<i32> duplicated;
    Array<i32> allEqual;

    struct Timings
    {
        const char* name;

        Array<u64> sortedTimes;
        Array<r64> sortedMillis;
        Array<u64> reversedTimes;
        Array<r64> reversedMillis;
        Array<u64> randomTimes;
        Array<r64> randomMillis;
        Array<u64> duplicatedTimes;
        Array<r64> duplicatedMillis;
        Array<u64> allEqualTimes;
        Array<r64> allEqualMillis;
    };
};

internal SortingBenchmark
SetUpSortingBenchmark( u32 N, bool ascending, MemoryArena* tmpArena )
{
    SortingBenchmark result = {};
    result.tmpArena = tmpArena;
    result.ascending = ascending;

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
    result.sortedMillis = NewArray<r64>( passes );
    result.reversedMillis = NewArray<r64>( passes );
    result.randomMillis = NewArray<r64>( passes );
    result.duplicatedMillis = NewArray<r64>( passes );
    result.allEqualMillis = NewArray<r64>( passes );

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

    DeleteArray( timings->sortedMillis );
    DeleteArray( timings->reversedMillis );
    DeleteArray( timings->randomMillis );
    DeleteArray( timings->duplicatedMillis );
    DeleteArray( timings->allEqualMillis );
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
TestSortingBenchmark( SortingBenchmark* benchmark, u32 passes )
{
#if 0
    if( benchmark->sorted.count < 10000 )
    {
        SortingBenchmark::Timings insertionSort = InitTimings( "Insertion Sort", passes );
        for( u32 i = 0; i < passes; ++i )
        {
            Array<i32> sorted = CopyArray( benchmark->sorted );
            insertionSort.sortedTimes[i] = TIME( InsertSort( &sorted, benchmark->ascending ) );
            EXPECT_TRUE( VerifySorted( sorted, benchmark->ascending ) );
            //printf( "%s (sorted, %u) :: %llu cycles\n", insertionSort.name, benchmark->sorted.count, insertionSort.sortedTimes[i] );
        }
        printf( "%s (sorted, %u) :: %llu cycles\n", insertionSort.name, benchmark->sorted.count, Average( insertionSort.sortedTimes ) );
    }
#endif

    SortingBenchmark::Timings quickSort = InitTimings( "Quick Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        Array<i32> sorted = CopyArray( benchmark->sorted );
        StartCounter();
        quickSort.sortedTimes[i] = TIME( QuickSort( &sorted, benchmark->ascending ) );
        quickSort.sortedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( sorted, benchmark->ascending ) );

        Array<i32> reversed = CopyArray( benchmark->reversed );
        StartCounter();
        quickSort.reversedTimes[i] = TIME( QuickSort( &reversed, benchmark->ascending ) );
        quickSort.reversedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( reversed, benchmark->ascending ) );

        Array<i32> random = CopyArray( benchmark->random );
        StartCounter();
        quickSort.randomTimes[i] = TIME( QuickSort( &random, benchmark->ascending ) );
        quickSort.randomMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( random, benchmark->ascending ) );

        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        StartCounter();
        quickSort.duplicatedTimes[i] = TIME( QuickSort( &duplicated, benchmark->ascending ) );
        quickSort.duplicatedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( duplicated, benchmark->ascending ) );

        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        StartCounter();
        quickSort.allEqualTimes[i] = TIME( QuickSort( &allEqual, benchmark->ascending ) );
        quickSort.allEqualMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( allEqual, benchmark->ascending ) );
    }
    printf( "%s (sorted,     %8u) :: %12llu cycles :: %7.3f ms\n", quickSort.name, benchmark->sorted.count, Average( quickSort.sortedTimes ), Average( quickSort.sortedMillis ) );
    printf( "%s (reversed,   %8u) :: %12llu cycles :: %7.3f ms\n", quickSort.name, benchmark->reversed.count, Average( quickSort.reversedTimes ), Average( quickSort.reversedMillis ) );
    printf( "%s (random,     %8u) :: %12llu cycles :: %7.3f ms\n", quickSort.name, benchmark->random.count, Average( quickSort.randomTimes ), Average( quickSort.randomMillis ) );
    printf( "%s (duplicated, %8u) :: %12llu cycles :: %7.3f ms\n", quickSort.name, benchmark->duplicated.count, Average( quickSort.duplicatedTimes ), Average( quickSort.duplicatedMillis ) );
    printf( "%s (allEqual,   %8u) :: %12llu cycles :: %7.3f ms\n", quickSort.name, benchmark->allEqual.count, Average( quickSort.allEqualTimes ), Average( quickSort.allEqualMillis ) );

    printf( "---\n" );

    DeleteTimings( &quickSort );

    SortingBenchmark::Timings radixSort = InitTimings( "Radix Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        ClearArena( benchmark->tmpArena );
        Array<i32> sorted = CopyArray( benchmark->sorted );
        StartCounter();
        radixSort.sortedTimes[i] = TIME( RadixSort( &sorted, benchmark->ascending, benchmark->tmpArena ) );
        radixSort.sortedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( sorted, benchmark->ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> reversed = CopyArray( benchmark->reversed );
        StartCounter();
        radixSort.reversedTimes[i] = TIME( RadixSort( &reversed, benchmark->ascending, benchmark->tmpArena ) );
        radixSort.reversedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( reversed, benchmark->ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> random = CopyArray( benchmark->random );
        StartCounter();
        radixSort.randomTimes[i] = TIME( RadixSort( &random, benchmark->ascending, benchmark->tmpArena ) );
        radixSort.randomMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( random, benchmark->ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        StartCounter();
        radixSort.duplicatedTimes[i] = TIME( RadixSort( &duplicated, benchmark->ascending, benchmark->tmpArena ) );
        radixSort.duplicatedMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( duplicated, benchmark->ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        StartCounter();
        radixSort.allEqualTimes[i] = TIME( RadixSort( &allEqual, benchmark->ascending, benchmark->tmpArena ) );
        radixSort.allEqualMillis[i] = GetCounter();
        EXPECT_TRUE( VerifySorted( allEqual, benchmark->ascending ) );
    }
    printf( "%s (sorted,     %8u) :: %12llu cycles :: %7.3f ms\n", radixSort.name, benchmark->sorted.count, Average( radixSort.sortedTimes ), Average( radixSort.sortedMillis ) );
    printf( "%s (reversed,   %8u) :: %12llu cycles :: %7.3f ms\n", radixSort.name, benchmark->reversed.count, Average( radixSort.reversedTimes ), Average( radixSort.reversedMillis ) );
    printf( "%s (random,     %8u) :: %12llu cycles :: %7.3f ms\n", radixSort.name, benchmark->random.count, Average( radixSort.randomTimes ), Average( radixSort.randomMillis ) );
    printf( "%s (duplicated, %8u) :: %12llu cycles :: %7.3f ms\n", radixSort.name, benchmark->duplicated.count, Average( radixSort.duplicatedTimes ), Average( radixSort.duplicatedMillis ) );
    printf( "%s (allEqual,   %8u) :: %12llu cycles :: %7.3f ms\n", radixSort.name, benchmark->allEqual.count, Average( radixSort.allEqualTimes ), Average( radixSort.allEqualMillis ) );

    printf( "\n" );
    printf( "---\n" );
    printf( "\n" );
}




void
main( int argC, char** argV )
{
    RandomSeed();

    LARGE_INTEGER li;
    ASSERT_TRUE( QueryPerformanceFrequency( &li ) );
    globalCounterFreq = r64(li.QuadPart) / 1000.0;

    MemoryArena tmpArena;
    u32 memorySize = 4*1024*1024;
    InitArena( &tmpArena, new u8[memorySize], memorySize );

    bool testSortingBenchmark = true;

    if( testSortingBenchmark )
    {
        SortingBenchmark sortingBenchmark = SetUpSortingBenchmark( 100, false, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10 );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10 );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 10000, false, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10 );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000000, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10 );
        TearDownSortingBenchmark( &sortingBenchmark );
    }
}
