#ifndef __PLATFORM_H__
#define __PLATFORM_H__ 


//
// Compiler stuff
//

#if _MSC_VER
#define COMPILER_MSVC 1
#else
// TODO(casey): Moar compilerz!!!
#define COMPILER_LLVM 1
#endif

//
// Exports
//

#ifdef _WIN32
#define LIB_EXPORT extern "C" __declspec(dllexport)
#else
#define LIB_EXPORT extern "C"
#endif

//
// Common definitions
//

#define global static
#define internal static
#define local_persistent static

#if DEBUG
#define ASSERT(expression) if( !(expression) ) { *(int *)0 = 0; }
#else
#define ASSERT(expression)
#endif

#if DEBUG
#define NOT_IMPLEMENTED ASSERT(!"NotImplemented")
#else
#define NOT_IMPLEMENTED NotImplemented!!!
#endif

#define INVALID_CODE_PATH ASSERT("!InvalidCodePath")
#define INVALID_DEFAULT_CASE default: { INVALID_CODE_PATH; } break


#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES(value)*1024)


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;
typedef float r32;
typedef double r64;


#endif /* __PLATFORM_H__ */
