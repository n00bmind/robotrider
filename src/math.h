#ifndef __MATH_H__
#define __MATH_H__ 

#define PI32 3.141592653589f


inline u32
Ceil( r64 value )
{
    u32 result = (u32)(value + 0.5);
    return result;
}

#endif /* __MATH_H__ */
