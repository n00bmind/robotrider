#ifndef __UTIL_H__
#define __UTIL_H__ 

enum class RadixKey
{
    U32 = 0,
    I32,
    R32,
    U64,
    I64,
    R64,
};

struct KeyIndex
{
    u32 key;
    u32 index;
};

struct KeyIndex64
{
    u64 key;
    u32 index;
    u32 _padding;
};

#endif /* __UTIL_H__ */
