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
#define TIME(f) ( __rdtsc_start = __rdtsc(), f, __rdtsc() - __rdtsc_start )

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
    return result;
}

template <typename T>
internal void DeleteArray( Array<T>& array )
{
    delete[] array.data;
    array.data = nullptr;
}



struct SortingBenchmark
{
    Array<i32> sorted;
    Array<i32> reversed;
    Array<i32> random;
    Array<i32> duplicated;

    struct Timings
    {
        const char* name;

        Array<u64> sortedTimes;
        Array<u64> reversedTimes;
        Array<u64> randomTimes;
        Array<u64> duplicatedTimes;
    };
};

internal SortingBenchmark
SetUpSortingBenchmark( u32 N, bool ascending )
{
    SortingBenchmark result = {};

    // TODO Ensure there's a fair (configurable) amount of duplicate elements

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

    return result;
}

internal void
TearDownSortingBenchmark( SortingBenchmark* benchmark )
{
    DeleteArray( benchmark->sorted );
    DeleteArray( benchmark->reversed );
    DeleteArray( benchmark->random );
    DeleteArray( benchmark->duplicated );
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

    return result;
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
    SortingBenchmark::Timings insertionSort = InitTimings( "Insertion Sort", passes );
    for( u32 i = 0; i < insertionSort.sortedTimes.maxCount; ++i )
    {
        Array<i32> sorted = CopyArray( benchmark->sorted );
        insertionSort.sortedTimes[i] = TIME( InsertSort( &sorted, ascending ) );
        EXPECT_TRUE( VerifySorted( sorted, ascending ) );

        printf( "%s (sorted) :: %llu cycles\n", insertionSort.name, insertionSort.sortedTimes[i] );
    }
}




void
main( int argC, char** argV )
{
    RandomSeed();

    bool testSortingBenchmark = true;

    if( testSortingBenchmark )
    {
        SortingBenchmark sortingBenchmark = SetUpSortingBenchmark( 100, true );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000, true );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 10000, true );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 100000, true );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000000, true );
        TestSortingBenchmark( &sortingBenchmark, 10, true );
        TearDownSortingBenchmark( &sortingBenchmark );
    }
}
