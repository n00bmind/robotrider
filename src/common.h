#ifndef __DEFS_H__
#define __DEFS_H__ 

//
// Common definitions
//

#define internal static
#define persistent static


#define HALT() ( (*(volatile int *)0x0BAD = 0) != 0 )
#if !RELEASE
#define ASSERT(expr) ((void)( !(expr) && (globalAssertHandler( #expr, __FILE__, __LINE__ ), 1) && HALT()))
#define ASSERTM(expr, msg) ((void)( !(expr) && (globalAssertHandler( msg, __FILE__, __LINE__ ), 1) && HALT()))
#else
#define ASSERT(expr) ((void)0)
#define ASSERTM(expr, msg) ((void)0)
#endif

#define ASSERT_HANDLER(name) void name( const char* msg, const char* file, int line )
typedef ASSERT_HANDLER(AssertHandlerFunc);

ASSERT_HANDLER(DefaultAssertHandler);
AssertHandlerFunc* globalAssertHandler = DefaultAssertHandler;

#if !RELEASE
#define NOT_IMPLEMENTED ASSERT(!"NotImplemented")
#else
#define NOT_IMPLEMENTED NotImplemented!!!
#endif

#define INVALID_CODE_PATH ASSERT(!"InvalidCodePath");
#define INVALID_DEFAULT_CASE default: { INVALID_CODE_PATH; } break;


#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))
#define OFFSETOF(type, member) ((sz)&(((type *)0)->member))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES((u64)value)*1024)

#define COPY(source, dest, size) memcpy( dest, source, size )
#define SET(dest, value, size) memset( dest, value, size )
#define ZERO(dest, size) memset( dest, 0, size )


// NOTE Only use these where storage is important (i.e: structs)
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef unsigned long long u64;

typedef float r32;
typedef double r64;

typedef size_t sz;

#define U8MAX UINT8_MAX
#define I32MAX INT32_MAX
#define I32MIN INT32_MIN
#define U32MAX UINT32_MAX

#define R32MAX FLT_MAX
#define R32MIN FLT_MIN
#define R32INF INFINITY
#define R32NAN NAN
#define R64INF (r64)INFINITY


#endif /* __DEFS_H__ */
