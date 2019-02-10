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

#ifndef __MATH_TYPES_H__
#define __MATH_TYPES_H__ 

/// We start with a quite standard right-handed cartesian coordinate system in which
///     +X goes right
///     +Y goes up
///     +Z goes towards the viewer
///
/// However, the robotrider world uses a slight variation of this system in which
/// your dude starts up sliding along over the XY plane, and so, in this scenario,
/// Y is 'forward' and Z is 'up'.
///
/// When referring to Euler angles, 'pitch' is the rotation about the +X axis, 'yaw' is
/// the rotation about the +Z ('up') axis, and 'roll' is the rotation about the +Y
/// ('forward') axis, and they're applied in that order.
/// Rotation angles go in the direction determined by the right hand screw rule,
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

const v2i V2iZero = { 0, 0 };

inline v2i
V2i( i32 x, i32 y )
{
    v2i result = { x, y };
    return result;
}

inline bool
operator ==( const v2i& a, const v2i& b )
{
    return a.x == b.x && a.y == b.y;
}

inline v2i
operator +( const v2i& a, const v2i& b )
{
    v2i result = { a.x + b.x, a.y + b.y };
    return result;
}

inline v2i
Hadamard( const v2i& a, const v2i& b )
{
    v2i result = { a.x * b.x, a.y * b.y };
    return result;
}

// Vector 2 unsigned

union v2u
{
    struct
    {
        u32 x, y;
    };
    u32 e[2];
};

inline v2u
V2u( u32 x, u32 y )
{
    v2u result = { x, y };
    return result;
}

inline v2u
V2u( const v2i& v )
{
    ASSERT( v.x >= 0 && v.y >= 0 );
    v2u result = { (u32)v.x, (u32)v.y };
    return result;
}

inline v2i
V2i( const v2u& v )
{
    ASSERT( v.x <= I32MAX && v.y <= I32MAX );
    v2i result = { (i32)v.x, (i32)v.y };
    return result;
}

inline v2u
Hadamard( const v2u& a, const v2u& b )
{
    v2u result = { a.x * b.x, a.y * b.y };
    return result;
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

const v2 V2Zero = { 0, 0 };

inline v2
V2( r32 x, r32 y )
{
    v2 result = { x, y };
    return result;
}

inline v2
V2( const v2i &v )
{
    v2 result = { (r32)v.x, (r32)v.y };
    return result;
}

inline bool
operator ==( const v2& a, const v2& b )
{
    return a.x == b.x && a.y == b.y;
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

// Vector 3 integer

union v3i
{
    struct
    {
        i32 x, y, z;
    };
    struct
    {
        v2i xy;
        i32 _ignored0;
    };
    struct
    {
        i32 _ignored1;
        v2i yz;
    };
    i32 e[3];
};

inline v3i
V3i( i32 x, i32 y, i32 z )
{
    v3i result = { x, y, z };
    return result;
}

inline bool
operator ==( const v3i &a, const v3i &b )
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool
operator !=( const v3i &a, const v3i &b )
{
    return a.x != b.x || a.y != b.y || a.z != b.z;
}

inline v3i
operator +( const v3i &a, const v3i &b )
{
    v3i result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

inline v3i
operator -( const v3i &a, const v3i &b )
{
    v3i result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}

// Vector 3 unsigned

union v3u
{
    struct
    {
        u32 x, y, z;
    };
    struct
    {
        v2u xy;
        i32 _ignored0;
    };
    struct
    {
        i32 _ignored1;
        v2u yz;
    };
    u32 e[3];
};

inline v3u
V3u( u32 x, u32 y, u32 z )
{
    v3u result = { x, y, z };
    return result;
}

inline v3u
V3u( u32 s )
{
    v3u result = { s, s, s };
    return result;
}

inline bool
operator ==( const v3u &a, const v3u &b )
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool
operator !=( const v3u &a, const v3u &b )
{
    return a.x != b.x || a.y != b.y || a.z != b.z;
}

inline v3u
operator +( const v3u &a, const v3u &b )
{
    v3u result = { a.x + b.x, a.y + b.y, a.z + b.z };
    return result;
}

inline v3u
operator -( const v3u &a, const v3u &b )
{
    v3u result = { a.x - b.x, a.y - b.y, a.z - b.z };
    return result;
}

inline v3u
Hadamard( const v3u& a, const v3u& b )
{
    v3u result = { a.x * b.x, a.y * b.y, a.z * b.z };
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

const v3 V3Zero = { 0.0f, 0.0f, 0.0f };
const v3 V3Undefined = { R32NAN, R32NAN, R32NAN };
// Canonical world orientations
// We define our right-handed game world as having the positive Z axis pointing up
// (consequently the positive Y axis points 'forward')
const v3 V3X = { 1.0f, 0.0f, 0.0f };
const v3 V3Y = { 0.0f, 1.0f, 0.0f };
const v3 V3Z = { 0.0f, 0.0f, 1.0f };
const v3 V3Right   = { 1.0f, 0.0f, 0.0f };
const v3 V3Forward = { 0.0f, 1.0f, 0.0f };
const v3 V3Up      = { 0.0f, 0.0f, 1.0f };

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
V3( const v3i& v )
{
    v3 result = { (r32)v.x, (r32)v.y, (r32)v.z };
    return result;
}

inline bool
operator ==( const v3 &a, const v3 &b )
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

inline bool
AlmostEqual( const v3& a, const v3& b, r32 absoluteEpsilon = 0 )
{
    return AlmostEqual( a.x, b.x, absoluteEpsilon )
        && AlmostEqual( a.y, b.y, absoluteEpsilon )
        && AlmostEqual( a.z, b.z, absoluteEpsilon );
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

inline void
operator *=( v3& v, r32 s )
{
    v.x *= s;
    v.y *= s;
    v.z *= s;
}

inline v3
operator *( const v3i &v, r32 s )
{
    v3 result = { (r32)v.x * s, (r32)v.y * s, (r32)v.z * s };
    return result;
}

inline v3
operator *( r32 s, const v3 &v )
{
    v3 result = { v.x * s, v.y * s, v.z * s };
    return result;
}

inline v3
operator /( const v3& v, r32 s )
{
    v3 result = { v.x / s, v.y / s, v.z / s };
    return result;
}

inline r32
Dot( const v3& a, const v3& b )
{
    r32 result = a.x * b.x + a.y * b.y + a.z * b.z;
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

inline r32
Length( const v3& v )
{
    r32 result = Sqrt( Dot( v, v ) );
    return result;
}

inline r32
LengthSq( const v3& v )
{
    r32 result = Dot( v, v );
    return result;
}

inline bool
IsUnit( const v3& v )
{
    return AlmostEqual( LengthSq( v ), 1.f );
}

inline r32
Distance( const v3& a, const v3& b )
{
    r32 result = Length(a - b);
    return result;
}

inline r32
DistanceSq( const v3& a, const v3& b )
{
    r32 result = LengthSq(a - b);
    return result;
}

inline void
Normalize( v3& v )
{
    if( !IsUnit( v ) )
    {
        r32 invL = 1.0f / Sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
        v *= invL;
    }
}

inline v3
Normalized( const v3 &v )
{
    v3 result = v;
    if( !IsUnit( v ) )
    {
        r32 invL = 1.0f / Sqrt( v.x * v.x + v.y * v.y + v.z * v.z );
        result = v * invL;
    }
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

inline v4
operator /( const v4& v, r32 s )
{
    v4 result = { v.x / s, v.y / s, v.z / s, v.w / s };
    return result;
}

inline v4
Hadamard( const v4& a, const v4& b )
{
    v4 result = { a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w };
    return result;
}

inline u32
Pack01ToRGBA( const v4& c )
{
    u32 result = (((Round( c.a * 255 ) & 0xFF) << 24)
                | ((Round( c.b * 255 ) & 0xFF) << 16)
                | ((Round( c.g * 255 ) & 0xFF) << 8)
                |  (Round( c.r * 255 ) & 0xFF));
    return result;
}

inline v4
UnpackRGBAToV401( u32 c )
{
    v4 result =
    {
        ((c >>  0) & 0xFF) / 255.f,
        ((c >>  8) & 0xFF) / 255.f,
        ((c >> 16) & 0xFF) / 255.f,
        ((c >> 24) & 0xFF) / 255.f,
    };
    return result;
}


// Matrix 4x4

union m4
{
    r32 e[4][4];
};

const m4 M4Identity =
{{
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 },
    { 0, 0, 0, 1 }
}};

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

inline m4
M4Basis( const v3& x, const v3& y, const v3& z )
{
    return M4Columns( x, y, z );
}

inline m4
M4CameraBasis( const v3 &x, const v3 &y, const v3 &z )
{
    return M4Rows( x, y, z );
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

inline m4
M4XRotation( r32 pitchAngleRads )
{
    r32 s = Sin( pitchAngleRads );
    r32 c = Cos( pitchAngleRads );

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
M4YRotation( r32 rollAngleRads )
{
    r32 s = Sin( rollAngleRads );
    r32 c = Cos( rollAngleRads );

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
M4ZRotation( r32 yawAngleRads )
{
    r32 s = Sin( yawAngleRads );
    r32 c = Cos( yawAngleRads );

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
M4Scale( const v3 &f )
{
    m4 result =
    {{
         { f.x,   0,    0,    0 },
         { 0,     f.y,  0,    0 },
         { 0,     0,    f.z,  0 },
         { 0,     0,    0,    1 }
    }};

    return result;
}

inline m4
M4RotPos( const m4 &m, const v3 &p )
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

// TODO SIMD this! (and most of the other stuff!)
inline v3
Transform( const m4 &m, const v3 &v )
{
    // FIXME This seems to take a good chunk of time!
    //TIMED_BLOCK;

    v3 r;
    r.x = v.x*m.e[0][0] + v.y*m.e[0][1] + v.z*m.e[0][2] + m.e[0][3];
    r.y = v.x*m.e[1][0] + v.y*m.e[1][1] + v.z*m.e[1][2] + m.e[1][3];
    r.z = v.x*m.e[2][0] + v.y*m.e[2][1] + v.z*m.e[2][2] + m.e[2][3];
    
    return r;
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
    v3 r = Transform( m, v );
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
    
    for(int r = 0; r < 4; ++r)
    {
        for(int c = 0; c < 4; ++c)
        {
            for(int i = 0; i < 4; ++i)
            {
                result.e[r][c] += m1.e[r][i] * m2.e[i][c];
            }
        }
    }
    
    return result;
}

inline void
operator *=( m4& m1, const m4& m2 )
{
    m1 = m1 * m2;
}

inline m4
Translate( m4 &m, const v3& vDelta )
{
    m.e[0][3] += vDelta.x;
    m.e[1][3] += vDelta.y;
    m.e[2][3] += vDelta.z;

    return m;
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
M4CameraTransform( const v3 &x, const v3 &y, const v3 &z, const v3 &p )
{
    m4 r = M4Rows( x, y, z );
    Translate( r, -(r*p) );
    return r;
}

inline m4
M4CameraTransform( const m4& rot, const v3& p )
{
    m4 result = GetRotation( rot );
    Translate( result, -(result * p) );
    return result;
}

inline m4
M4CameraLookAt( const v3 &pSrc, const v3 &pTgt, const v3 &vUp )
{
    v3 vUpN = Normalized( vUp );
    v3 vZ = Normalized( -(pTgt - pSrc) );
    
    v3 vX = V3X;
    v3 vY = V3Y;
    if( AlmostEqual( vUpN, vZ ) )
        ; // No-op
    else if( AlmostEqual( -vUpN, vZ ) )
        vY = -V3Y;
    else
    {
        vX = Cross( vUpN, vZ );
        vY = Cross( vZ, vX );
    }

    m4 r = M4CameraTransform( vX, vY, vZ, pSrc );
    return r;
}

inline m4
M4CameraLookAtDir( const v3 &pSrc, const v3 &vDir, const v3 &vUp )
{
    v3 vUpN = Normalized( vUp );
    v3 vZ = Normalized( -vDir );

    v3 vX = V3X;
    v3 vY = V3Y;
    if( AlmostEqual( vUpN, vZ ) )
        ; // No-op
    else if( AlmostEqual( -vUpN, vZ ) )
        vY = -V3Y;
    else
    {
        vX = Cross( vUpN, vZ );
        vY = Cross( vZ, vX );
    }

    m4 r = M4CameraTransform( vX, vY, vZ, pSrc );
    return r;
}

inline m4
M4Perspective( r32 aspectRatio, r32 fovYDeg )
{
    r32 n = 0.1f;		// Make this configurable?
    r32 f = 1000.0f;
    r32 d = f - n;
    r32 a = aspectRatio;
    r32 fovy = Radians( fovYDeg );
    r32 ctf = 1 / (r32)tan( fovy / 2 );

    m4 result =
    {{
        { ctf/a,      0,           0,            0 },
        {     0,    ctf,           0,            0 },
        {     0,      0,    -(f+n)/d,     -2*f*n/d },
        {     0,      0,          -1,            0 } 
    }};

    return result;
}

inline m4
M4Orthographic( r32 width, r32 height )
{
    r32 w = width;
    r32 h = -height;

    m4 result =
    {{
        { 2.0f/w,        0,      0,     0 },
        {      0,   2.0f/h,      0,     0 },
        {      0,        0,     -1,     0 },
        {     -1,        1,      0,     1 },
    }};

    return result;
}

inline bool
AlmostEqual( const m4& a, const m4& b )
{
    for( int r = 0; r < 4; ++r )
        for( int c = 0; c < 4; ++c )
            // C is row-major
            if( !AlmostEqual( a.e[r][c], b.e[r][c] ) )
                return false;
    return true;
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

inline v4
GetRow( const m4 &m, u32 row )
{
    ASSERT( row >= 0 && row < 4 );
    v4 result = { m.e[row][0], m.e[row][1], m.e[row][2], m.e[row][3] };
    return result;
}

inline v4
GetColumn( const m4 &m, u32 col )
{
    ASSERT( col >= 0 && col < 4 );
    v4 result = { m.e[0][col], m.e[1][col], m.e[2][col], m.e[3][col] };
    return result;
}

inline v3
GetBasisX( const m4& m )
{
    v3 result = { m.e[0][0], m.e[1][0], m.e[2][0] };
    return result;
}

inline v3
GetBasisY( const m4& m )
{
    v3 result = { m.e[0][1], m.e[1][1], m.e[2][1] };
    return result;
}

inline v3
GetBasisZ( const m4& m )
{
    v3 result = { m.e[0][2], m.e[1][2], m.e[2][2] };
    return result;
}

inline void
GetBasis( const m4& m, v3* vX, v3* vY, v3* vZ )
{
    *vX = { m.e[0][0], m.e[1][0], m.e[2][0] };
    *vY = { m.e[0][1], m.e[1][1], m.e[2][1] };
    *vZ = { m.e[0][2], m.e[1][2], m.e[2][2] };
}

inline v3
GetCameraBasisX( const m4& m )
{
    v3 result = { m.e[0][0], m.e[0][1], m.e[0][2] };
    return result;
}

inline v3
GetCameraBasisY( const m4& m )
{
    v3 result = { m.e[1][0], m.e[1][1], m.e[1][2] };
    return result;
}

inline v3
GetCameraBasisZ( const m4& m )
{
    v3 result = { m.e[2][0], m.e[2][1], m.e[2][2] };
    return result;
}

inline void
GetCameraBasis( const m4& m, v3* vX, v3* vY, v3* vZ )
{
    *vX = { m.e[0][0], m.e[0][1], m.e[0][2] };
    *vY = { m.e[1][0], m.e[1][1], m.e[1][2] };
    *vZ = { m.e[2][0], m.e[2][1], m.e[2][2] };
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



// Symmetric Matrix 4x4 (double precision)

union m4Symmetric
{
    // TODO Try if it would be acceptable to use just floats here
    r64 e[10];
};

const m4Symmetric M4SymmetricZero = {0};

// Make plane
inline m4Symmetric
M4Symmetric( r64 a, r64 b, r64 c, r64 d )
{
    m4Symmetric result =
    {{
        a * a, a * b, a * c, a * d,
               b * b, b * c, b * d,
                      c * c, c * d,
                             d * d,
     }};

    return result;
}

inline m4Symmetric
operator +( const m4Symmetric& a, const m4Symmetric& b )
{
    m4Symmetric result =
    {{
         a.e[0] + b.e[0], a.e[1] + b.e[1], a.e[2] + b.e[2], a.e[3] + b.e[3],
                          a.e[4] + b.e[4], a.e[5] + b.e[5], a.e[6] + b.e[6],
                                           a.e[7] + b.e[7], a.e[8] + b.e[8],
                                                            a.e[9] + b.e[9],
    }};

    return result;
}

inline void
operator +=( m4Symmetric& a, const m4Symmetric& b )
{
    a.e[0] += b.e[0]; a.e[1] += b.e[1]; a.e[2] += b.e[2]; a.e[3] += b.e[3];
                      a.e[4] += b.e[4]; a.e[5] += b.e[5]; a.e[6] += b.e[6];
                                        a.e[7] += b.e[7]; a.e[8] += b.e[8];
                                                          a.e[9] += b.e[9];
}

inline r64
Determinant3x3( const m4Symmetric& m,
                int e11, int e12, int e13,
                int e21, int e22, int e23, 
                int e31, int e32, int e33 )
{
    r64 result = m.e[e11]*m.e[e22]*m.e[e33] + m.e[e13]*m.e[e21]*m.e[e32] + m.e[e12]*m.e[e23]*m.e[e31]
               - m.e[e13]*m.e[e22]*m.e[e31] - m.e[e11]*m.e[e23]*m.e[e32] - m.e[e12]*m.e[e21]*m.e[e33];

    return result;
}

// Quaternion

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

const qn QnIdentity = { 0.0f, 0.0f, 0.0f, 1.0f };

// TODO (Unit) Test everything quaternion related

inline r32
SqNorm( const qn& q )
{
    r32 result = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return result;
}

inline r32
Norm( const qn& q )
{
    return Sqrt( SqNorm( q ) );
}

inline void
Normalize( qn& q )
{
    r32 invNorm = 1.0f / Norm( q );
    q.x *= invNorm;
    q.y *= invNorm;
    q.z *= invNorm;
    q.w *= invNorm;
}

inline qn
Qn( const v3& vAxis, r32 angleRads )
{
    v3 vAxisN = Normalized( vAxis );

    r32 halfAngleRads = angleRads * 0.5f;
    r32 sinTheta = Sin( halfAngleRads );
    r32 cosTheta = Cos( halfAngleRads );

    qn result =
    {
        vAxisN.x * sinTheta,
        vAxisN.y * sinTheta,
        vAxisN.z * sinTheta,
        cosTheta
    };
    //r32 norm = Norm( result );

    return result;
}

inline qn
QnXRotation( r32 pitchAngleRads )
{
    v3 vAngle = V3X;
    qn result = Qn( vAngle, pitchAngleRads );
    r32 norm = Norm( result );
    return result;
}

inline qn
QnYRotation( r32 rollAngleRads )
{
    v3 vAngle = V3Y;
    qn result = Qn( vAngle, rollAngleRads );
    return result;
}

inline qn
QnZRotation( r32 yawAngleRads )
{
    v3 vAngle = V3Z;
    qn result = Qn( vAngle, yawAngleRads );
    return result;
}

inline qn
Qn( const m4 &m )
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

inline qn
QnCameraLookAt( const v3 &pSrc, const v3 &pTgt, const v3 &vUp )
{
    // TODO How to enforce this?
    v3 vUpN = Normalized( vUp );

    v3 vTargetFwdN = Normalized( pTgt - pSrc );
    v3 vCamFwdDefaultN = V3( 0, 0, -1 );

    r32 theta = PI;
    v3 vRotAxis = { 1, 0, 0 };
    qn result = QnIdentity;

    if( AlmostEqual( vTargetFwdN, vCamFwdDefaultN ) )
        // No-op
        ;
    else if( AlmostEqual( -vTargetFwdN, vCamFwdDefaultN ) )
    {
        result = Qn( vRotAxis, theta );
    }
    else
    {
        r32 cosTheta = Dot( vCamFwdDefaultN, vTargetFwdN );
        // The camera transform is actually the transform we would apply to the _world_ to put it in front of the camera,
        // so we invert the angle here..
        // @Speed Find a way to build the quat directly without this
        theta = -ACos( cosTheta );
        vRotAxis = Cross( vCamFwdDefaultN, vTargetFwdN );
        vRotAxis.y = 0;
        vRotAxis = Normalized( vRotAxis );

        result = Qn( vRotAxis, theta );
    }

    return result;
}

inline m4
ToM4( const qn& q )
{
    r64 sqw = q.w * q.w;
    r64 sqx = q.x * q.x;
    r64 sqy = q.y * q.y;
    r64 sqz = q.z * q.z;

    m4 result = {};
    r64 invs = 1.0;
    r64 sqNorm = sqx + sqy + sqz + sqw;
    if( !AlmostEqual( sqNorm, 1.0 ) )
    {
        // Inverse square lenght, only required if quaternion is not already normalized (multiply all matrix terms by it)
        invs = 1.0 / sqNorm;
    }

    result.e[0][0] = r32(( sqx - sqy - sqz + sqw) * invs);
    result.e[1][1] = r32((-sqx + sqy - sqz + sqw) * invs);
    result.e[2][2] = r32((-sqx - sqy + sqz + sqw) * invs);

    r64 tmp1 = q.x * q.y;
    r64 tmp2 = q.z * q.w;
    result.e[1][0] = r32(2.0 * (tmp1 + tmp2) * invs);
    result.e[0][1] = r32(2.0 * (tmp1 - tmp2) * invs);
    
    tmp1 = q.x * q.z;
    tmp2 = q.y * q.w;
    result.e[2][0] = r32(2.0 * (tmp1 - tmp2) * invs);
    result.e[0][2] = r32(2.0 * (tmp1 + tmp2) * invs);

    tmp1 = q.y * q.z;
    tmp2 = q.x * q.w;
    result.e[2][1] = r32(2.0 * (tmp1 + tmp2) * invs);
    result.e[1][2] = r32(2.0 * (tmp1 - tmp2) * invs);

    result.e[3][3] = 1.f;
    return result;
}

inline bool
IsUnit( const qn& q )
{
    return AlmostEqual( SqNorm( q ), 1.f, 1e-05f );
}

inline qn
Conjugate( const qn& q )
{
    qn result = { -q.x, -q.y, -q.z, q.w };
    return result;
}

inline qn
Inverse( const qn& q )
{
    qn result = Conjugate( q );
    if( !IsUnit( q ) )
    {
        r32 invSqNorm = 1.0f / SqNorm( q );
        result.x *= invSqNorm;
        result.y *= invSqNorm;
        result.z *= invSqNorm;
        result.w *= invSqNorm;
    }
    return result;
}

inline qn
operator *( const qn& a, const qn& b )
{
    r32 rw = b.w*a.w - b.x*a.x - b.y*a.y - b.z*a.z;
    r32 rx = b.w*a.x + b.x*a.w - b.y*a.z + b.z*a.y;
    r32 ry = b.w*a.y + b.x*a.z + b.y*a.w - b.z*a.x;
    r32 rz = b.w*a.z - b.x*a.y + b.y*a.x + b.z*a.w;
    qn result = { rx, ry, rz, rw };
    return result;
}

inline void
operator *=( qn& a, const qn& b )
{
    a = a * b;
}

inline v3
Rotate( const v3& v, const qn& q )
{
    ASSERT( IsUnit( q ) );

    // TODO Optimize
    qn vPure = { v.x, v.y, v.z, 0 };
    qn qResult = q * vPure * Conjugate( q );
    return qResult.xyz;
}



// Triangle

struct tri
{
    union
    {
        struct
        {
            v3 v0, v1, v2;
        };
        v3 v[3];
    };
    v3 n;
};

inline tri
Tri( const v3& v0, const v3& v1, const v3& v2, bool findNormal = false )
{
    tri result = { v0, v1, v2 };

    if( findNormal )
        result.n = Normalized( Cross( v1 - v0, v2 - v0 ) );

    return result;
}


// AABB

struct aabb
{
    r32 xMin, xMax;
    r32 yMin, yMax;
    r32 zMin, zMax;
};


// Ray

struct ray
{
    v3 p;
    v3 dir;
};

inline bool
Intersects( const ray& r, tri& t, v3* pI = nullptr, r32 absoluteEpsilon = 0 )
{
    bool result = false;

    v3 u = t.v1 - t.v0;
    v3 v = t.v2 - t.v0;

    if( t.n == V3Zero )
        t.n = Normalized( Cross( u, v ) );
    ASSERT( !AlmostEqual( t.n, V3Zero ) );

    r32 denom = Dot( t.n, r.dir );
    if( AlmostEqual( denom, 0 ) )
    {
        // Check if ray is coplanar
        if( AlmostEqual( Distance( r.p, t.v0 ), 0 ) )
        {
            if( pI )
                *pI = V3Undefined;
            result = true;
        }
    }
    else
    {
        v3 dist = t.v0 - r.p;
        r32 num = Dot( t.n, dist );
        r32 rDist = num / denom;

        if( AlmostEqual( rDist, 0 ) )
        {
            // Super edge case
            if( pI )
                *pI = r.p;
            result = true;
        }
        else if( rDist > 0 )
        {
            v3 i = r.p + rDist * r.dir;

            if( pI )
                *pI = i;

            // Is it inside?
            r32 uu = Dot( u, u );
            r32 vv = Dot( v, v );
            r32 uv = Dot( u, v );
            denom = uv * uv - uu * vv;

            v3 w = i - t.v0;
            r32 wu = Dot( w, u );
            r32 wv = Dot( w, v );

            r32 sI = (uv * wv - vv * wu) / denom;
            if( GreaterOrAlmostEqual( sI, 0, absoluteEpsilon ) && LessOrAlmostEqual( sI, 1, absoluteEpsilon ) )
            {
                r32 tI = (uv * wu - uu * wv) / denom;
                if( GreaterOrAlmostEqual( tI, 0, absoluteEpsilon ) && LessOrAlmostEqual( sI + tI, 1, absoluteEpsilon ) )
                {
                    result = true;
                }
            }
        }
    }

    return result;
}

#endif /* __MATH_TYPES_H__ */
