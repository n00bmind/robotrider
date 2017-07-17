#ifndef __MATH_H__
#define __MATH_H__ 

#include "math_types.h"
#define PI32 3.141592653589f
#define PI64 3.14159265358979323846


inline u32
SafeTruncU64( u64 value )
{
    ASSERT( value <= 0xFFFFFFFF );
    u32 result = (u32)value;
    return result;
}

inline u32
Ceil( r64 value )
{
    u32 result = (u32)(value + 0.5);
    return result;
}

inline r32
Radians( r32 degrees )
{
    r32 result = degrees * PI32 / 180;
    return result;
}
#endif /* __MATH_H__ */
