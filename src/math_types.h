#ifndef __MATH_TYPES_H__
#define __MATH_TYPES_H__ 


union v2
{
    struct
    {
        r32 x, y;
    };
    struct
    {
        r32 u, v;
    };
    r32 e[2];
};

union v3
{
    struct
    {
        r32 x, y, z;
    };
    struct
    {
        r32 u, v, __;
    };
    struct
    {
        r32 r, g, b;
    };
    struct
    {
        v2 xy;
        r32 _ignored0;
    };
    struct
    {
        r32 _ignored1;
        v2 yz;
    };
    struct
    {
        v2 uv;
        r32 _ignored2;
    };
    r32 e[3];
};

union v4
{
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                r32 x, y, z;
            };
        };
        
        r32 w;
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                r32 r, g, b;
            };
        };
        
        r32 a;
    };
    struct
    {
        v2 xy;
        r32 _ignored0;
        r32 _ignored1;
    };
    struct
    {
        r32 _ignored2;
        v2 yz;
        r32 _ignored3;
    };
    struct
    {
        r32 _ignored4;
        r32 _ignored5;
        v2 zw;
    };
    r32 e[4];
};

union m4
{
    r32 e[4][4];
};



inline m4
M4Identity()
{
    m4 result =
    {{
         { 1, 0, 0, 0 },
         { 0, 1, 0, 0 },
         { 0, 0, 1, 0 },
         { 0, 0, 0, 1 }
    }};

    return result;
}

inline m4
M4Translation( v3 p )
{
    m4 result =
    {{
         { 1,   0,  0,  p.x },
         { 0,   1,  0,  p.y },
         { 0,   0,  1,  p.z },
         { 0,   0,  0,  1 }
    }};

    return result;
}

#endif /* __MATH_TYPES_H__ */
