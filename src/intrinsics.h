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

#ifndef __INTRINSICS_H__
#define __INTRINSICS_H__ 

#if _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif


// TODO Convert all of these to the most platform-efficient versions
// for all supported compilers & platforms
// TODO Examine disassemblies for all compilers and compare!

inline u32
SafeTruncToU32( u64 value )
{
    ASSERT( value <= 0xFFFFFFFF );
    u32 result = (u32)value;
    return result;
}

inline u32
Ceil( r32 value )
{
    u32 result = (u32)(value + 0.5f);
    return result;
}

inline u32
Ceil( r64 value )
{
    u32 result = (u32)(value + 0.5);
    return result;
}

inline u32
Round( r32 value )
{
    u32 result = (u32)roundf( value );
    return result;
}

inline u32
Round( r64 value )
{
    u32 result = (u32)round( value );
    return result;
}

inline r32
Sin( r32 angleRads )
{
    r32 result = sinf( angleRads );
    return result;
}

inline r32
Cos( r32 angleRads )
{
    r32 result = cosf( angleRads );
    return result;
}

inline r32
Sqrt( r32 value )
{
    r32 result = sqrtf( value );
    return result;
}

inline r32
Min( r32 a, r32 b )
{
    return a < b ? a : b;
}

inline r64
Min( r64 a, r64 b )
{
    return a < b ? a : b;
}

inline u64
Min( u64 a, u64 b )
{
    return a < b ? a : b;
}

inline r32
Max( r32 a, r32 b )
{
    return a > b ? a : b;
}

inline r32
Clamp0( r32 value )
{
    return Max( 0, value );
}

inline i32
Square( i32 value )
{
    return value * value;
}

inline r64
Pow( r64 b, r64 exp )
{
    return pow( b, exp );
}

inline r32
Abs( r32 value )
{
    return value >= 0.f ? value : -value;
}

inline u32
GetNextPowerOf2( u32 value )
{
    u32 result = 0;

#if _MSC_VER
    unsigned long msbPosition;
    if( _BitScanReverse( &msbPosition, value ) )
    {
        result = 1 << (msbPosition + 1);
    }
#else
    u32 leadingZeros = _lzcnt_u32( value );
    if( leadingZeros < 32 )
    {
        result = 1 << (32 - leadingZeros);
    }
#endif

    return result;
}

inline u32
AtomicCompareExchangeU32( volatile u32* value, u32 newValue, u32 expectedValue )
{
    u32 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedCompareExchange( (volatile long*)value,
                                                 newValue, expectedValue );
#else
    __atomic_compare_exchange_n( value, &expectedValue, newValue, false,
                                 __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST );
    // The (copy of) expectedValue will be overwritten with the actual value if they were different
    previousValue = expectedValue;
#endif

    return previousValue;
}

inline u64
AtomicExchangeU64( volatile u64* value, u64 newValue )
{
    u64 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchange64( (volatile i64*)value, newValue );
#else
    previousValue = __atomic_exchange_n( value, newValue, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline u32
AtomicAddU32( volatile u32* value, u32 addend )
{
    u32 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchangeAdd( (volatile long*)value, addend );
#else
    previousValue = __atomic_fetch_add( value, addend, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline u64
AtomicAddU64( volatile u64* value, u64 addend )
{
    u64 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchangeAdd64( (volatile i64*)value, addend );
#else
    previousValue = __atomic_fetch_add( value, addend, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

#endif /* __INTRINSICS_H__ */
