#ifndef __MATH_TYPES_H__
#define __MATH_TYPES_H__ 

/// We use a quite standard right-handed cartesian coordinate system in which
///     +X goes right
///     +Y goes up
///     +Z goes towards the viewer
///
/// However, the robotrider 'world' uses a slight variation of this system in which
/// your dude starts up sliding along over the XY plane, and so, in this scenario,
/// Y is 'forward' and Z is 'up'.
///
/// When referring to Euler angles, 'pitch' is the rotation about the +X axis, 'yaw' is
/// the rotation about the +Z ('up') axis, and 'roll' is the rotation about the +Y
/// ('forward') axis, and they're applied in that order.
/// Position rotation angles go in the direction determined by the right hand screw rule,
/// as any sane person knows.

// TODO IMPORTANT Write a nice test suite for this whole file


// Vector 2 integer

union v2i
{
    struct
    {
        i32 x, y;
    };
    i32 e[2];
};

inline v2i
V2iZero()
{
    return { 0, 0 };
}

inline bool
operator ==( const v2i &a, const v2i &b )
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
V2Zero()
{
    return { 0.0f, 0.0f };
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
V3Zero()
{
    return { 0.0f, 0.0f, 0.0f };
}

inline bool
operator ==( const v3 &a, const v3 &b )
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
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

inline void
operator +=( v3 &a, const v3 &b )
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
}

inline v3
operator *( const v3 &v, r32 s )
{
    v3 result = { v.x * s, v.y * s, v.z * s };
    return result;
}

inline v3
operator *( r32 s, const v3 &v )
{
    v3 result = { v.x * s, v.y * s, v.z * s };
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

// Canonical world orientations
// We define our game world as having the positive Z axis pointing up
// (consequently the positive Y axis points 'forward')

inline v3
V3X()
{
    v3 result = { 1.0f, 0.0f, 0.0f };
    return result;
}

inline v3
V3Right()
{
    v3 result = { 1.0f, 0.0f, 0.0f };
    return result;
}

inline v3
V3Y()
{
    v3 result = { 0.0f, 1.0f, 0.0f };
    return result;
}

inline v3
V3Forward()
{
    v3 result = { 0.0f, 1.0f, 0.0f };
    return result;
}

inline v3
V3Z()
{
    v3 result = { 0.0f, 0.0f, 1.0f };
    return result;
}

inline v3
V3Up()
{
    v3 result = { 0.0f, 0.0f, 1.0f };
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
V4( r32 x, r32 y, r32 z, r32 w )
{
    v4 result = { x, y, z, w };
    return result;
}

inline v4
V4( const v3 &v, r32 w )
{
    v4 result = { v.x, v.y, v.z, w };
    return result;
}

inline v4
operator *( r32 f, const v4 &v )
{
    v4 result = { f * v.x, f * v.y, f * v.z, f * v.w };
    return result;
}

// Matrix 4x4

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
M4Translation( const v3 &p )
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
M4Rows( const v3 &x, const v3 &y, const v3 &z )
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
M4Columns( const v3 &x, const v3 &y, const v3 &z )
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

inline m4
Transposed( const m4 &m )
{
    m4 result =
    {{
         { m.e[0][0], m.e[1][0], m.e[2][0], m.e[3][0] },
         { m.e[0][1], m.e[1][1], m.e[2][1], m.e[3][1] },
         { m.e[0][2], m.e[1][2], m.e[2][2], m.e[3][2] },
         { m.e[0][3], m.e[1][3], m.e[2][3], m.e[3][3] },
    }};

    return result;
}

inline v4
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

inline m4
CameraTransform( const v3 &x, const v3 &y, const v3 &z, const v3 &p )
{
    m4 r = M4Rows( x, y, z );
    r = Translate( r, -(r*p) );

    return r;
}

inline m4
CameraTransform( const m4 &rot, const v3 &p )
{
    m4 r = GetRotation( rot );
    r = Translate( r, -(r*p) );

    return r;
}

inline m4
CameraLookAt( const v3 &pSrc, const v3 &pTgt, const v3 &vUp )
{
    v3 vUpN = Normalized( vUp );
    v3 vZ = Normalized( pSrc - pTgt );
    v3 vX = Cross( vUpN, vZ );
    v3 vY = Cross( vZ, vX );

    m4 r = M4Rows( vX, vY, vZ );
    r = Translate( r, -(r*pSrc) );

    return r;
}

inline m4
CameraLookAtDir( const v3 &pSrc, const v3 &vDir, const v3 &vUp )
{
    v3 vUpN = Normalized( vUp );
    v3 vZ = Normalized( -vDir );
    v3 vX = Cross( vUpN, vZ );
    v3 vY = Cross( vZ, vX );

    m4 r = M4Rows( vX, vY, vZ );
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

union qn
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

// TODO Test
inline qn
QN( const m4 &m )
{
    qn r;
    float tr = m.e[0][0] + m.e[1][1] + m.e[2][2];

    if( tr > 0.f )
    {
        float s = (float)(sqrt( tr + 1.0 ) * 2);     // s = 4 * qw
        r.w = 0.25f * s;
        r.x = (m.e[2][1] - m.e[1][2]) / s;
        r.y = (m.e[0][2] - m.e[2][0]) / s;
        r.z = (m.e[1][0] - m.e[0][1]) / s;
    }
    else if( (m.e[0][0] > m.e[1][1]) && (m.e[0][0] > m.e[2][2]) )
    {
        float s = (float)(sqrt( 1.0 + m.e[0][0] - m.e[1][1] - m.e[2][2] ) * 2);  // s = 4 * qx
        r.w = (m.e[2][1] - m.e[1][2]) / s;
        r.x = 0.25f * s;
        r.y = (m.e[0][1] + m.e[1][0]) / s;
        r.z = (m.e[0][2] + m.e[2][0]) / s;
    }
    else if( m.e[1][1] > m.e[2][2] )
    {
        float s = (float)(sqrt( 1.0 + m.e[1][1] - m.e[0][0] - m.e[2][2]) * 2);   // s = 4 * qy
        r.w = (m.e[0][2] - m.e[2][0]) / s;
        r.x = (m.e[0][1] + m.e[1][0]) / s;
        r.y = 0.25f * s;
        r.z = (m.e[1][2] + m.e[2][1]) / s;
    }
    else
    {
        float s = (float)(sqrt( 1.0 + m.e[2][2] - m.e[0][0] - m.e[1][1]) * 2);   // s = 4 * qz
        r.w = (m.e[1][0] - m.e[0][1]) / s;
        r.x = (m.e[0][2] + m.e[2][0]) / s;
        r.y = (m.e[1][2] + m.e[2][1]) / s;
        r.z = 0.25f * s;
    }

    return r;
}

inline void
ToEulerXYZ( const qn &r, float *pitch, float *yaw, float *roll )
{
    // TODO Develop the editor a bit further so that these ops can be applied
    // over a selected object so we can visually test them at least
}

#endif /* __MATH_TYPES_H__ */
