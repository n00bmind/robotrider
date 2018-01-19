#ifndef __MATH_H__
#define __MATH_H__ 

#include "intrinsics.h"
#include "math_types.h"
#define PI32 3.141592653589f
#define PI64 3.14159265358979323846

inline r32
Radians( r32 degrees )
{
    r32 result = degrees * PI32 / 180;
    return result;
}

inline u32
RGBAPack( v4 unpacked )
{
    u32 result = (((Round(unpacked.a) & 0xFF) << 24)
                | ((Round(unpacked.b) & 0xFF) << 16)
                | ((Round(unpacked.g) & 0xFF) << 8)
                | ((Round(unpacked.r) & 0xFF) << 0));
    return result;
}
#endif /* __MATH_H__ */
