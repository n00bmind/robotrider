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

// TODO Add support for different log levels (like in Android) and categories/filters
#define LOG platform.Log

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

typedef size_t mem_idx;


enum class Renderer
{
    OpenGL,
    // TODO OpenGLES,
    // Software?
};


//
// Services that the platform layer provides to the game
//


struct DEBUGReadFileResult
{
    u32 contentSize;
    void *contents;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name( char *filename )
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DebugPlatformReadEntireFileFunc);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name( void *memory )
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DebugPlatformFreeFileMemoryFunc);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name( char *filename, u32 memorySize, void *memory )
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DebugPlatformWriteEntireFileFunc);

#define PLATFORM_LOG(name) void name( const char *fmt, ... )
typedef PLATFORM_LOG(PlatformLogFunc);


struct PlatformAPI
{
    DebugPlatformReadEntireFileFunc *DEBUGReadEntireFile;
    DebugPlatformFreeFileMemoryFunc *DEBUGFreeFileMemory;
    DebugPlatformWriteEntireFileFunc *DEBUGWriteEntireFile;

    PlatformLogFunc *Log;
};
extern PlatformAPI platform;


#endif /* __PLATFORM_H__ */
