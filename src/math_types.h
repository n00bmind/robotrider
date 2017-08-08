#ifndef __MATH_TYPES_H__
#define __MATH_TYPES_H__ 


// Vector 2 integer

union v2i
{
    struct
    {
        i32 x, y;
    };
    i32 e[2];
};

inline bool
AreEqual( const v2i &a, const v2i &b )
{
    return a.x == b.x && a.y == b.y;
}

// Vector 2

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

inline v2
V2( const v2i &v )
{
    v2 result = { (r32)v.x, (r32)v.y };
    return result;
}

inline v2
operator -( const v2 &a, const v2 &b )
{
    v2 result = { a.x - b.x, a.y - b.y };
    return result;
}

inline r32
LengthSq( const v2 &v )
{
    r32 result = v.x * v.x + v.y * v.y;
    return result;
}

inline v2i
Round( const v2 &v )
{
    v2i result = { (i32)v.x, (i32)v.y };
    return result;
}

// Vector 3

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

inline v3
V3( r32 x, r32 y, r32 z )
{
    v3 result = { x, y, z };
    return result;
}

inline v3
V3( const v2 &v, r32 z )
{
    v3 result = { v.x, v.y, z };
    return result;
}

inline v3
operator -( const v3 &v )
{
    v3 result = { -v.x, -v.y, -v.z };
    return result;
}

inline v3
operator +( const v3 &a, const v3 &b )
{
    v3 result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

inline v3
operator -( const v3 &a, const v3 &b )
{
    v3 result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}

inline v3
Cross( const v3 &a, const v3 &b )
{
    v3 result =
    {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return result;
}

inline v3
Normalized( const v3 &v )
{
    r32 invL = 1.0f / Sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
    v3 result = { v.x * invL, v.y * invL, v.z * invL };
    return result;
}

// Vector 4

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

inline v4
V4( const v3 &v, r32 w )
{
    v4 result = { v.x, v.y, v.z, w };
    return result;
}


// Matrix 4x4

union m4
{
    r32 e[4][4];
};

inline m4
Identity()
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
Translation( const v3 &p )
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

inline v3
GetTranslation( const m4 &m )
{
    v3 result = { m.e[0][3], m.e[1][3], m.e[2][3] };
    return result;
}

inline void
SetTranslation( m4 &m, const v3 &p )
{
    m.e[0][3] = p.x;
    m.e[1][3] = p.y;
    m.e[2][3] = p.z;
}

inline m4
Translate( m4 &m, const v3 &v )
{
    m.e[0][3] += v.x;
    m.e[1][3] += v.y;
    m.e[2][3] += v.z;

    return m;
}

inline m4
XRotation( r32 angleRads )
{
    r32 s = Sin( angleRads );
    r32 c = Cos( angleRads );

    m4 result =
    {{
         { 1, 0, 0, 0 },
         { 0, c,-s, 0 },
         { 0, s, c, 0 },
         { 0, 0, 0, 1 }
    }};

    return result;
}

inline m4
YRotation( r32 angleRads )
{
    r32 s = Sin( angleRads );
    r32 c = Cos( angleRads );

    m4 result =
    {{
         { c, 0, s, 0 },
         { 0, 1, 0, 0 },
         {-s, 0, c, 0 },
         { 0, 0, 0, 1 }
    }};

    return result;
}

inline m4
ZRotation( r32 angleRads )
{
    r32 s = Sin( angleRads );
    r32 c = Cos( angleRads );

    m4 result =
    {{
         { c,-s, 0, 0 },
         { s, c, 0, 0 },
         { 0, 0, 1, 0 },
         { 0, 0, 0, 1 }
    }};

    return result;
}

inline m4
GetRotation( const m4 &m )
{
    m4 result =
    {{
         { m.e[0][0], m.e[0][1], m.e[0][2], 0 },
         { m.e[1][0], m.e[1][1], m.e[1][2], 0 },
         { m.e[2][0], m.e[2][1], m.e[2][2], 0 },
         {         0,         0,         0, 1 }
    }};

    return result;
}

inline m4
Rows( const v3 &x, const v3 &y, const v3 &z )
{
    m4 result =
    {{
         { x.x, x.y, x.z, 0 },
         { y.x, y.y, y.z, 0 },
         { z.x, z.y, z.z, 0 },
         {   0,   0,   0, 1 }
    }};

    return result;
}

inline v4
GetRow( const m4 &m, u32 row )
{
    ASSERT( row >= 0 && row < 4 );
    v4 result = { m.e[row][0], m.e[row][1], m.e[row][2], m.e[row][3] };
    return result;
}

inline m4
Columns( const v3 &x, const v3 &y, const v3 &z )
{
    m4 result =
    {{
         { x.x, y.x, z.x, 0 },
         { x.y, y.y, z.y, 0 },
         { x.z, y.z, z.z, 0 },
         {   0,   0,   0, 1 }
    }};

    return result;
}

inline v4
GetColumn( const m4 &m, u32 col )
{
    ASSERT( col >= 0 && col < 4 );
    v4 result = { m.e[0][col], m.e[1][col], m.e[2][col], m.e[3][col] };
    return result;
}

internal v4
Transform( const m4 &m, const v4 &v )
{
    v4 r;
    r.x = v.x*m.e[0][0] + v.y*m.e[0][1] + v.z*m.e[0][2] + v.w*m.e[0][3];
    r.y = v.x*m.e[1][0] + v.y*m.e[1][1] + v.z*m.e[1][2] + v.w*m.e[1][3];
    r.z = v.x*m.e[2][0] + v.y*m.e[2][1] + v.z*m.e[2][2] + v.w*m.e[2][3];
    r.w = v.x*m.e[3][0] + v.y*m.e[3][1] + v.z*m.e[3][2] + v.w*m.e[3][3];
    
    return r;
}

inline v3
operator*( const m4 &m, const v3 &v )
{
    v3 r = Transform( m, V4( v, 1.0f ) ).xyz;
    return r;
}

inline v4
operator*( const m4 &m, const v4 &v )
{
    v4 r = Transform( m, v );
    return r;
}

inline m4
operator*( const m4 &m1, const m4 &m2 )
{
    m4 result = {};
    
    for(int r = 0; r <= 3; ++r)
    {
        for(int c = 0; c <= 3; ++c)
        {
            for(int i = 0; i <= 3; ++i)
            {
                result.e[r][c] += m1.e[r][i] * m2.e[i][c];
            }
        }
    }
    
    return result;
}

internal m4
CameraTransform( const v3 &x, const v3 &y, const v3 &z, const v3 &p )
{
    m4 r = Rows( x, y, z );
    r = Translate( r, -(r*p) );

    return r;
}

internal m4
CameraLookAt( const v3 &pSrc, const v3 &pTgt, const v3 &vUp )
{
    v3 vUpN = Normalized( vUp );
    v3 vZ = Normalized( pSrc - pTgt );
    v3 vX = Cross( vUpN, vZ );
    v3 vY = Cross( vZ, vX );

    m4 r = Rows( vX, vY, vZ );
    r = Translate( r, -(r*pSrc) );

    return r;
}

inline m4
RotPos( const m4 &m, const v3 &p )
{
    m4 result =
    {{
         { m.e[0][0], m.e[0][1], m.e[0][2], p.x },
         { m.e[1][0], m.e[1][1], m.e[1][2], p.y },
         { m.e[2][0], m.e[2][1], m.e[2][2], p.z },
         {         0,         0,         0,   1 }
    }};

    return result;
}

// Quaternions

union q
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
    r32 e[4];
};

#endif /* __MATH_TYPES_H__ */
