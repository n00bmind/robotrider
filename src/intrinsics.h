#ifndef __INTRINSICS_H__
#define __INTRINSICS_H__ 

// TODO Convert all of these to the most platform-efficient versions


inline u32
SafeTruncToU32( u64 value )
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
Sin(r32 angleRads)
{
    r32 result = sinf( angleRads );
    return result;
}

inline r32
Cos(r32 angleRads)
{
    r32 result = cosf( angleRads );
    return result;
}


#endif /* __INTRINSICS_H__ */
