#ifndef __UTIL_H__
#define __UTIL_H__ 

#if NON_UNITY_BUILD
#include "common.h"
#include "data_types.h"
#endif


enum class RadixKey
{
    U32 = 0,
    I32,
    F32,
    U64,
    I64,
    F64,
};

struct KeyIndex
{
    u32 key;
    i32 index;
};

struct KeyIndex64
{
    u64 key;
    i32 index;
    u32 _padding;
};



void RadixSort( Array<KeyIndex64>* inputOutput, RadixKey keyType, bool ascending, MemoryArena* tmpArena );
template <typename T> void BuildSortableKeysArray( const Array<T>& sourceTypeArray, sz typeKeyOffset, Array<KeyIndex64>* result );

#endif /* __UTIL_H__ */
