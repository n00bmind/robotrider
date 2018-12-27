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
#include "math.h"
#include "math_types.h"
#include "data_types.h"
#include "util.h"
#include "util.cpp"


internal r64 globalCounterFreqSecs = 0.0;
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
    Array<T> result = Array<T>( new T[n], 0, n );
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
GetCounterMs()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    return r64(li.QuadPart - globalCounterStart) / globalCounterFreqSecs * 1000.0;
}

inline r64
GetCounterUs()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter( &li );
    return r64(li.QuadPart - globalCounterStart) / globalCounterFreqSecs * 1000000.0;
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
    Array<i32> smallish;
    Array<r32> randomFloat;
    Array<i64> randomLong;
    Array<r64> randomDouble;
    Array<KeyIndex64> randomStruct;

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
        Array<u64> smallishTimes;
        Array<r64> smallishMillis;
        Array<u64> randomFloatTimes;
        Array<r64> randomFloatMillis;
        Array<u64> randomLongTimes;
        Array<r64> randomLongMillis;
        Array<u64> randomDoubleTimes;
        Array<r64> randomDoubleMillis;
        Array<u64> randomStructTimes;
        Array<r64> randomStructMillis;
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

    {
        // Only smallish-ish values
        result.smallish = NewArray<i32>( N );
        for( u32 i = 0; i < result.smallish.maxCount; ++i )
        {
            result.smallish[i] = RandomI32() >> 11;
        }
    }

    {
        // Random float
        result.randomFloat = NewArray<r32>( N );
        for( u32 i = 0; i < result.randomFloat.maxCount; ++i )
        {
            result.randomFloat[i] = RandomRangeR32( -10000, 10000 );
        }
    }

    {
        // Random long
        result.randomLong = NewArray<i64>( N );
        for( u32 i = 0; i < result.randomLong.maxCount; ++i )
        {
            result.randomLong[i] = RandomI64();
        }
    }

    {
        // Random double
        result.randomDouble = NewArray<r64>( N );
        for( u32 i = 0; i < result.randomDouble.maxCount; ++i )
        {
            result.randomDouble[i] = RandomRangeR64( -1000000000, 100000000000 );
        }
    }

    {
        // Random struct
        result.randomStruct = NewArray<KeyIndex64>( N );
        for( u32 i = 0; i < result.randomStruct.maxCount; ++i )
        {
            result.randomStruct[i] = { RandomU64(), i };
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
    DeleteArray( benchmark->allEqual );
    DeleteArray( benchmark->smallish );
    DeleteArray( benchmark->randomFloat );
    DeleteArray( benchmark->randomLong );
    DeleteArray( benchmark->randomDouble );
    DeleteArray( benchmark->randomStruct );

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
    result.smallishTimes = NewArray<u64>( passes );
    result.randomFloatTimes = NewArray<u64>( passes );
    result.randomLongTimes = NewArray<u64>( passes );
    result.randomDoubleTimes = NewArray<u64>( passes );
    result.randomStructTimes = NewArray<u64>( passes );

    result.sortedMillis = NewArray<r64>( passes );
    result.reversedMillis = NewArray<r64>( passes );
    result.randomMillis = NewArray<r64>( passes );
    result.duplicatedMillis = NewArray<r64>( passes );
    result.allEqualMillis = NewArray<r64>( passes );
    result.smallishMillis = NewArray<r64>( passes );
    result.randomFloatMillis = NewArray<r64>( passes );
    result.randomLongMillis = NewArray<r64>( passes );
    result.randomDoubleMillis = NewArray<r64>( passes );
    result.randomStructMillis = NewArray<r64>( passes );

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
    DeleteArray( timings->smallishTimes );
    DeleteArray( timings->randomFloatTimes );
    DeleteArray( timings->randomLongTimes );
    DeleteArray( timings->randomDoubleTimes );
    DeleteArray( timings->randomStructTimes );

    DeleteArray( timings->sortedMillis );
    DeleteArray( timings->reversedMillis );
    DeleteArray( timings->randomMillis );
    DeleteArray( timings->duplicatedMillis );
    DeleteArray( timings->allEqualMillis );
    DeleteArray( timings->smallishMillis );
    DeleteArray( timings->randomFloatMillis );
    DeleteArray( timings->randomLongMillis );
    DeleteArray( timings->randomDoubleMillis );
    DeleteArray( timings->randomStructMillis );
}

// NOTE Only needed so that VerifySorted() compiles for this type
bool operator <=( const KeyIndex64& a, const KeyIndex64& b )
{
    return a.key <= b.key;
}
bool operator >=( const KeyIndex64& a, const KeyIndex64& b )
{
    return a.key >= b.key;
}

template <typename T>
internal bool
VerifySorted( const Array<T>& input, bool ascending )
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
#if 0
    SortingBenchmark::Timings quickSort = InitTimings( "Quick Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        Array<i32> sorted = CopyArray( benchmark->sorted );
        StartCounter();
        quickSort.sortedTimes[i] = TIME( QuickSort( &sorted, benchmark->ascending ) );
        quickSort.sortedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( sorted, benchmark->ascending ) );

        Array<i32> reversed = CopyArray( benchmark->reversed );
        StartCounter();
        quickSort.reversedTimes[i] = TIME( QuickSort( &reversed, benchmark->ascending ) );
        quickSort.reversedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( reversed, benchmark->ascending ) );

        Array<i32> random = CopyArray( benchmark->random );
        StartCounter();
        quickSort.randomTimes[i] = TIME( QuickSort( &random, benchmark->ascending ) );
        quickSort.randomMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( random, benchmark->ascending ) );

        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        StartCounter();
        quickSort.duplicatedTimes[i] = TIME( QuickSort( &duplicated, benchmark->ascending ) );
        quickSort.duplicatedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( duplicated, benchmark->ascending ) );

        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        StartCounter();
        quickSort.allEqualTimes[i] = TIME( QuickSort( &allEqual, benchmark->ascending ) );
        quickSort.allEqualMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( allEqual, benchmark->ascending ) );
    }
    printf( "%15s (sorted,     %8u) :: %12llu cycles :: %9.3f us\n", quickSort.name, benchmark->sorted.count, Average( quickSort.sortedTimes ), Average( quickSort.sortedMillis ) );
    printf( "%15s (reversed,   %8u) :: %12llu cycles :: %9.3f us\n", quickSort.name, benchmark->reversed.count, Average( quickSort.reversedTimes ), Average( quickSort.reversedMillis ) );
    printf( "%15s (random,     %8u) :: %12llu cycles :: %9.3f us\n", quickSort.name, benchmark->random.count, Average( quickSort.randomTimes ), Average( quickSort.randomMillis ) );
    printf( "%15s (duplicated, %8u) :: %12llu cycles :: %9.3f us\n", quickSort.name, benchmark->duplicated.count, Average( quickSort.duplicatedTimes ), Average( quickSort.duplicatedMillis ) );
    printf( "%15s (allEqual,   %8u) :: %12llu cycles :: %9.3f us\n", quickSort.name, benchmark->allEqual.count, Average( quickSort.allEqualTimes ), Average( quickSort.allEqualMillis ) );
    DeleteTimings( &quickSort );

    printf( "---\n" );
#endif

    SortingBenchmark::Timings radixSort = InitTimings( "Radix Sort", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        bool ascending = benchmark->ascending;

        ClearArena( benchmark->tmpArena );
        Array<i32> sorted = CopyArray( benchmark->sorted );
        StartCounter();
        radixSort.sortedTimes[i] = TIME( RadixSort( &sorted, ascending, benchmark->tmpArena ) );
        radixSort.sortedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( sorted, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> reversed = CopyArray( benchmark->reversed );
        StartCounter();
        radixSort.reversedTimes[i] = TIME( RadixSort( &reversed, ascending, benchmark->tmpArena ) );
        radixSort.reversedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( reversed, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> random = CopyArray( benchmark->random );
        StartCounter();
        radixSort.randomTimes[i] = TIME( RadixSort( &random, ascending, benchmark->tmpArena ) );
        radixSort.randomMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( random, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        StartCounter();
        radixSort.duplicatedTimes[i] = TIME( RadixSort( &duplicated, ascending, benchmark->tmpArena ) );
        radixSort.duplicatedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( duplicated, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        StartCounter();
        radixSort.allEqualTimes[i] = TIME( RadixSort( &allEqual, ascending, benchmark->tmpArena ) );
        radixSort.allEqualMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( allEqual, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> smallish = CopyArray( benchmark->smallish );
        StartCounter();
        radixSort.smallishTimes[i] = TIME( RadixSort( &smallish, ascending, benchmark->tmpArena ) );
        radixSort.smallishMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( smallish, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<r32> randomFloat = CopyArray( benchmark->randomFloat );
        StartCounter();
        radixSort.randomFloatTimes[i] = TIME( RadixSort( &randomFloat, ascending, benchmark->tmpArena ) );
        radixSort.randomFloatMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( randomFloat, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i64> randomLong = CopyArray( benchmark->randomLong );
        StartCounter();
        radixSort.randomLongTimes[i] = TIME( RadixSort( &randomLong, ascending, benchmark->tmpArena ) );
        radixSort.randomLongMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( randomLong, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<r64> randomDouble = CopyArray( benchmark->randomDouble );
        StartCounter();
        radixSort.randomDoubleTimes[i] = TIME( RadixSort( &randomDouble, ascending, benchmark->tmpArena ) );
        radixSort.randomDoubleMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( randomDouble, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<KeyIndex64> randomStruct = CopyArray( benchmark->randomStruct );
        StartCounter();
        radixSort.randomStructTimes[i] = TIME( RadixSort( &randomStruct, 0, RadixKey::U64, ascending, benchmark->tmpArena ) );
        radixSort.randomStructMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( randomStruct, ascending ) );
    }
    printf( "%15s (sorted,     %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->sorted.count, Average( radixSort.sortedTimes ), Average( radixSort.sortedMillis ) );
    printf( "%15s (reversed,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->reversed.count, Average( radixSort.reversedTimes ), Average( radixSort.reversedMillis ) );
    printf( "%15s (random,     %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->random.count, Average( radixSort.randomTimes ), Average( radixSort.randomMillis ) );
    printf( "%15s (duplicated, %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->duplicated.count, Average( radixSort.duplicatedTimes ), Average( radixSort.duplicatedMillis ) );
    printf( "%15s (allEqual,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->allEqual.count, Average( radixSort.allEqualTimes ), Average( radixSort.allEqualMillis ) );
    printf( "%15s (smallish,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->smallish.count, Average( radixSort.smallishTimes ), Average( radixSort.smallishMillis ) );
    printf( "%15s (randomFlt,  %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->randomFloat.count, Average( radixSort.randomFloatTimes ), Average( radixSort.randomFloatMillis ) );
    printf( "%15s (randomLong, %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->randomLong.count, Average( radixSort.randomLongTimes ), Average( radixSort.randomLongMillis ) );
    printf( "%15s (randomDbl,  %8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->randomDouble.count, Average( radixSort.randomDoubleTimes ), Average( radixSort.randomDoubleMillis ) );
    printf( "%15s (randomStrct,%8u) :: %12llu cycles :: %9.3f us\n", radixSort.name, benchmark->randomStruct.count, Average( radixSort.randomStructTimes ), Average( radixSort.randomStructMillis ) );
    DeleteTimings( &radixSort );

    printf( "---\n" );

    SortingBenchmark::Timings radixSort11 = InitTimings( "Radix Sort 11", passes );
    for( u32 i = 0; i < passes; ++i )
    {
        bool ascending = benchmark->ascending;

        ClearArena( benchmark->tmpArena );
        Array<i32> sorted = CopyArray( benchmark->sorted );
        StartCounter();
        radixSort11.sortedTimes[i] = TIME( RadixSort11( &sorted, ascending, benchmark->tmpArena ) );
        radixSort11.sortedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( sorted, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> reversed = CopyArray( benchmark->reversed );
        StartCounter();
        radixSort11.reversedTimes[i] = TIME( RadixSort11( &reversed, ascending, benchmark->tmpArena ) );
        radixSort11.reversedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( reversed, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> random = CopyArray( benchmark->random );
        StartCounter();
        radixSort11.randomTimes[i] = TIME( RadixSort11( &random, ascending, benchmark->tmpArena ) );
        radixSort11.randomMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( random, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> duplicated = CopyArray( benchmark->duplicated );
        StartCounter();
        radixSort11.duplicatedTimes[i] = TIME( RadixSort11( &duplicated, ascending, benchmark->tmpArena ) );
        radixSort11.duplicatedMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( duplicated, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> allEqual = CopyArray( benchmark->allEqual );
        StartCounter();
        radixSort11.allEqualTimes[i] = TIME( RadixSort11( &allEqual, ascending, benchmark->tmpArena ) );
        radixSort11.allEqualMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( allEqual, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<i32> smallish = CopyArray( benchmark->smallish );
        StartCounter();
        radixSort11.smallishTimes[i] = TIME( RadixSort11( &smallish, ascending, benchmark->tmpArena ) );
        radixSort11.smallishMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( smallish, ascending ) );

        ClearArena( benchmark->tmpArena );
        Array<r32> randomFloat = CopyArray( benchmark->randomFloat );
        StartCounter();
        radixSort11.randomFloatTimes[i] = TIME( RadixSort11( &randomFloat, ascending, benchmark->tmpArena ) );
        radixSort11.randomFloatMillis[i] = GetCounterUs();
        EXPECT_TRUE( VerifySorted( randomFloat, ascending ) );
    }
    printf( "%15s (sorted,     %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->sorted.count, Average( radixSort11.sortedTimes ), Average( radixSort11.sortedMillis ) );
    printf( "%15s (reversed,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->reversed.count, Average( radixSort11.reversedTimes ), Average( radixSort11.reversedMillis ) );
    printf( "%15s (random,     %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->random.count, Average( radixSort11.randomTimes ), Average( radixSort11.randomMillis ) );
    printf( "%15s (duplicated, %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->duplicated.count, Average( radixSort11.duplicatedTimes ), Average( radixSort11.duplicatedMillis ) );
    printf( "%15s (allEqual,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->allEqual.count, Average( radixSort11.allEqualTimes ), Average( radixSort11.allEqualMillis ) );
    printf( "%15s (smallish,   %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->smallish.count, Average( radixSort11.smallishTimes ), Average( radixSort11.smallishMillis ) );
    printf( "%15s (randomFlt,  %8u) :: %12llu cycles :: %9.3f us\n", radixSort11.name, benchmark->randomFloat.count, Average( radixSort11.randomFloatTimes ), Average( radixSort11.randomFloatMillis ) );
    DeleteTimings( &radixSort11 );

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
    globalCounterFreqSecs = r64(li.QuadPart);

    MemoryArena tmpArena;
    u32 memorySize = 16*1024*1024;
    InitArena( &tmpArena, new u8[memorySize], memorySize );

    bool testSortingBenchmark = true;

    if( testSortingBenchmark )
    {
        SortingBenchmark sortingBenchmark = SetUpSortingBenchmark( 100, true, &tmpArena );
        TestSortingBenchmark( &sortingBenchmark, 10 );
        TearDownSortingBenchmark( &sortingBenchmark );

        sortingBenchmark = SetUpSortingBenchmark( 1000, false, &tmpArena );
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
