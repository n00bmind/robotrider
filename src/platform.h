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

#define internal static
#define local_persistent static


#if DEBUG
#define HALT() ( (*(int *)0 = 0) != 0 )
#define ASSERT(expr) ((void)( !(expr) && (_assert_handler( #expr, __FILE__, __LINE__ ), 1) && HALT()))
#else
#define ASSERT(expr) ((void)0)
#endif

#if DEBUG
#define NOT_IMPLEMENTED ASSERT(!"NotImplemented")
#else
#define NOT_IMPLEMENTED NotImplemented!!!
#endif

#define INVALID_CODE_PATH ASSERT(!"InvalidCodePath");
#define INVALID_DEFAULT_CASE default: { INVALID_CODE_PATH; } break;

// TODO Add support for different log levels (like in Android) and categories/filters
#define LOG globalPlatform.Log

#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))
#define OFFSETOF(type, member) ((mem_idx)&(((type *)0)->member))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES(value)*1024)


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float r32;
typedef double r64;

typedef size_t mem_idx;



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

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name( char *filename, u32 memorySize, void *memory )
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
extern PlatformAPI globalPlatform;


typedef void (*AssertHandler)( const char *, const char *, int );

void DefaultAssertHandler( const char *expr, const char *file, int line )
{
    LOG( "ASSERTION FAILED! :: '%s'\n", expr );
}
AssertHandler _assert_handler = DefaultAssertHandler;


#endif /* __PLATFORM_H__ */