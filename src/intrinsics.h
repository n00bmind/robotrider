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
ToU32Safe( u64 value )
{
    ASSERT( value <= U32MAX );
    u32 result = (u32)value;
    return result;
}

inline u32
ToU32Safe( r64 value )
{
    ASSERT( value <= U32MAX );
    u32 result = (u32)value;
    return result;
}

inline u8
ToU8Safe( u32 value )
{
    ASSERT( value <= U8MAX );
    u8 result = (u8)value;
    return result;
}

inline u32
Ceil( r32 value )
{
    u32 result = (u32)ceilf( value );
    return result;
}

inline u32
Ceil( r64 value )
{
    u32 result = (u32)ceil( value );
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

inline i32
Min( i32 a, i32 b )
{
    return a < b ? a : b;
}

inline u32
Min( u32 a, u32 b )
{
    return a < b ? a : b;
}

inline u64
Min( u64 a, u64 b )
{
    return a < b ? a : b;
}

inline i32
Max( i32 a, i32 b )
{
    return a > b ? a : b;
}

inline r32
Max( r32 a, r32 b )
{
    return a > b ? a : b;
}

inline r64
Max( r64 a, r64 b )
{
    return a > b ? a : b;
}

inline i32
Median( i32 a, i32 b, i32 c )
{
    i32 result = Max( Min( a, b ), Min( Max( a, b ), c ) );
    return result;
}

inline r32
Clamp0( r32 value )
{
    return Max( 0.f, value );
}

inline i32
Square( i32 value )
{
    return value * value;
}

inline r32
Pow( r32 b, r32 exp )
{
    return powf( b, exp );
}

inline r64
PowR64( r64 b, r64 exp )
{
    return pow( b, exp );
}

inline r64
Log( r64 value )
{
    return log( value );
}

inline r32
Abs( r32 value )
{
    return value >= 0.f ? value : -value;
}

inline r64
Abs( r64 value )
{
    return value >= 0.0 ? value : -value;
}

inline void
Swap( i32* a, i32* b )
{
    i32 tmp = *a;
    *a = *b;
    *b = tmp;
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

// TODO What if the value already is a power of two?
inline u32
NextPowerOf2( u32 value )
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
AtomicCompareExchange( volatile u32* value, u32 newValue, u32 expectedValue )
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

inline u32
AtomicExchange( volatile u32* value, u32 newValue )
{
    u32 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchange( (volatile long*)value, newValue );
#else
    previousValue = __atomic_exchange_n( value, newValue, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline u64
AtomicExchange( volatile u64* value, u64 newValue )
{
    u64 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchange64( (volatile i64*)value, newValue );
#else
    previousValue = __atomic_exchange_n( value, newValue, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline bool
AtomicExchange( volatile bool* value, bool newValue )
{
    ASSERT( sizeof(bool) == sizeof(u8) );
    bool previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchange8( (volatile char*)value, newValue );
#else
    previousValue = __atomic_exchange_n( value, newValue, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline u32
AtomicLoad( volatile u32* value )
{
    u32 result = 0;
#if _MSC_VER
    result = _InterlockedOr( (volatile long*)value, 0 );
#else
    result = __atomic_load_n( value, __ATOMIC_SEQ_CST );
#endif

    return result;
}

inline bool
AtomicLoad( volatile bool* value )
{
    ASSERT( sizeof(bool) == sizeof(u8) );
    bool result = 0;
#if _MSC_VER
    result = _InterlockedOr8( (volatile char*)value, 0 );
#else
    result = __atomic_load_n( value, __ATOMIC_SEQ_CST );
#endif

    return result;
}

inline u32
AtomicAdd( volatile u32* value, u32 addend )
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
AtomicAdd( volatile u64* value, u64 addend )
{
    u64 previousValue = 0;
#if _MSC_VER
    previousValue = _InterlockedExchangeAdd64( (volatile i64*)value, addend );
#else
    previousValue = __atomic_fetch_add( value, addend, __ATOMIC_SEQ_CST );
#endif

    return previousValue;
}

inline u64
ReadCycles()
{
    // Flush the pipeline
    int cpuInfo[4];
    __cpuid( cpuInfo, 0 );
    return __rdtsc();
}

#endif /* __INTRINSICS_H__ */
