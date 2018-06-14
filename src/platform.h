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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__ 


//
// Compiler stuff
//

// TODO Properly test all this shiet

#if defined(__clang__) || defined(__GNUC__)   // Put this first so we account for clang-cl too

#define COMPILER_LLVM 1

#if defined(__amd64__) || defined(__x86_64__)
#define ARCH_X64 1
#elif defined(__arm__)
#define ARCH_ARM 1
#endif

#elif _MSC_VER

#define COMPILER_MSVC 1

#if defined(_M_X64) || defined(_M_AMD64)
#define ARCH_X64 1
#elif defined(_M_ARM)
#define ARCH_ARM 1
#endif

#else

#error Compiler not supported!

#endif //__clang__

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
#define HALT() ( (*(volatile int *)0 = 0) != 0 )
#define ASSERT(expr) ((void)( !(expr) && (_assert_handler( #expr, __FILE__, __LINE__ ), 1) && HALT()))
#define ASSERTM(expr, msg) ((void)( !(expr) && (_assert_handler( msg, __FILE__, __LINE__ ), 1) && HALT()))
#else
#define ASSERT(expr) ((void)0)
#define ASSERTM(expr, msg) ((void)0)
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
#define OFFSETOF(type, member) ((sz)&(((type *)0)->member))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES((u64)value)*1024)

#define CLEAR(dest, size, value) memset( dest, value, size )
#define CLEAR0(dest, size) memset( dest, 0, size )

// NOTE Only use these where storage is important (i.e: structs)
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float r32;
typedef double r64;

typedef size_t sz;

#define I32MAX INT32_MAX
#define I32MIN INT32_MIN
#define U32MAX UINT32_MAX

#define R32MAX FLT_MAX
#define R32MIN FLT_MIN
#define R32INF INFINITY
#define R32NAN NAN


#if COMPILER_MSVC
#define COMPILER_READ_BARRIER       _ReadBarrier();
#define COMPILER_WRITE_BARRIER      _WriteBarrier();
#define COMPILER_READWRITE_BARRIER  _ReadWriteBarrier();

#elif COMPILER_LLVM
#define COMPILER_READ_BARRIER       asm volatile("" ::: "memory");
#define COMPILER_WRITE_BARRIER      asm volatile("" ::: "memory");
#define COMPILER_READWRITE_BARRIER  asm volatile("" ::: "memory");

#endif

#if ARCH_X64

#define MEMORY_READ_BARRIER         /*_mm_lfence();*/COMPILER_READ_BARRIER      // Only store-load can be out of order in x64
#define MEMORY_WRITE_BARRIER        /*_mm_sfence();*/COMPILER_WRITE_BARRIER
#define MEMORY_READWRITE_BARRIER    _mm_mfence();COMPILER_READWRITE_BARRIER

#elif ARCH_ARM

// TODO 

#endif


//
// Services that the platform layer provides to the game
//


struct DEBUGReadFileResult
{
    u32 contentSize;
    void *contents;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name( const char *filename )
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DebugPlatformReadEntireFileFunc);

// TODO Remove
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name( void *memory )
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DebugPlatformFreeFileMemoryFunc);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name( const char *filename, u32 memorySize, void *memory )
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DebugPlatformWriteEntireFileFunc);


struct PlatformJobQueue;

#define PLATFORM_JOBQUEUE_CALLBACK(name) void name( void* userData, u32 workerThreadIndex )
typedef PLATFORM_JOBQUEUE_CALLBACK(PlatformJobQueueCallbackFunc);

#define PLATFORM_ADD_NEW_JOB(name) void name( PlatformJobQueue* queue, PlatformJobQueueCallbackFunc* callback, void* userData )
typedef PLATFORM_ADD_NEW_JOB(PlatformAddNewJobFunc);

#define PLATFORM_COMPLETE_ALL_JOBS(name) void name( PlatformJobQueue* queue )
typedef PLATFORM_COMPLETE_ALL_JOBS(PlatformCompleteAllJobsFunc);

#define PLATFORM_MAX_JOBQUEUE_JOBS 16768


#define PLATFORM_ALLOCATE_TEXTURE(name) void* name( u8* data, u32 width, u32 height )
typedef PLATFORM_ALLOCATE_TEXTURE(PlatformAllocateTextureFunc);

#define PLATFORM_DEALLOCATE_TEXTURE(name) void name( void* handle )
typedef PLATFORM_DEALLOCATE_TEXTURE(PlatformDeallocateTextureFunc);


#define PLATFORM_LOG(name) void name( const char *fmt, ... )
typedef PLATFORM_LOG(PlatformLogFunc);


struct DebugGameStats;

struct PlatformAPI
{
    DebugPlatformReadEntireFileFunc* DEBUGReadEntireFile;
    DebugPlatformFreeFileMemoryFunc* DEBUGFreeFileMemory;
    DebugPlatformWriteEntireFileFunc* DEBUGWriteEntireFile;

    PlatformAddNewJobFunc* AddNewJob;
    PlatformCompleteAllJobsFunc* CompleteAllJobs;
    PlatformJobQueue* hiPriorityQueue;
    //PlatformJobQueue* loPriorityQueue;
    u32 workerThreadsCount;

    PlatformAllocateTextureFunc* AllocateTexture;
    PlatformDeallocateTextureFunc* DeallocateTexture;

    PlatformLogFunc* Log;
};
extern PlatformAPI globalPlatform;


typedef void (*AssertHandler)( const char *, const char *, int );

void DefaultAssertHandler( const char *msg, const char *file, int line )
{
    LOG( "ASSERTION FAILED! :: '%s' (%s@%d)\n", msg, file, line );
}
AssertHandler _assert_handler = DefaultAssertHandler;


#endif /* __PLATFORM_H__ */
