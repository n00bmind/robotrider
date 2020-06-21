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



#ifdef _WIN32
#define LIB_EXPORT extern "C" __declspec(dllexport)
#else
#define LIB_EXPORT extern "C" __declspec(dllexport)
#endif

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

// TODO Add support for different log severities and categories/filters
#define LOG globalPlatform.Log



struct DEBUGReadFileResult
{
    void *contents;
    i32 contentSize;
};
// TODO Change this to use a given arena instead of allocating
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name( const char *filename )
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DebugPlatformReadEntireFileFunc);

// TODO Remove
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name( void *memory )
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DebugPlatformFreeFileMemoryFunc);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name( const char *filename, u32 memorySize, void *memory )
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DebugPlatformWriteEntireFileFunc);

#define PLATFORM_PATH_MAX 1024

// NOTE IMPORTANT The file info struct is for use in a DEBUG context only (in an editor mode, etc.)
// All asset information and content should be exclusively passed forward from the platform to the game
// and never requested back from the game to avoid dependecy loops in a runtime context as much as possible.
// In fact, this mechanism is something we should strive to eliminate entirely once the asset system
// is developed and can provide for all use cases we accomplish through here instead.
struct DEBUGFileInfo
{
    char fullPath[PLATFORM_PATH_MAX];
    const char* name;

    sz size;
    u64 lastUpdated;
    bool isFolder;
};

struct DEBUGFileInfoList
{
    DEBUGFileInfo* files;
    u32 entryCount;
};

struct MemoryArena;

// TODO Filters
#define DEBUG_PLATFORM_LIST_ALL_ASSETS(name) DEBUGFileInfoList name( const char* relativeRootPath, MemoryArena* arena )
typedef DEBUG_PLATFORM_LIST_ALL_ASSETS(DebugPlatformListAllAssetsFunc);

#define DEBUG_PLATFORM_JOIN_PATHS(name) sz name( const char *root, const char *path, char *destination, bool canonicalize )
typedef DEBUG_PLATFORM_JOIN_PATHS(DebugPlatformJoinPathsFunc);

#define DEBUG_PLATFORM_GET_PARENT_PATH(name) void name( const char* path, char* destination )
typedef DEBUG_PLATFORM_GET_PARENT_PATH(DebugPlatformGetParentPathFunc);


#define DEBUG_PLATFORM_CURRENT_TIME_MILLIS(name) f64 name()
typedef DEBUG_PLATFORM_CURRENT_TIME_MILLIS(DebugPlatformCurrentTimeMillis);


struct PlatformJobQueue;

#define PLATFORM_JOBQUEUE_CALLBACK(name) void name( void* userData, int workerThreadIndex )
typedef PLATFORM_JOBQUEUE_CALLBACK(PlatformJobQueueCallbackFunc);

#define PLATFORM_ADD_NEW_JOB(name) void name( PlatformJobQueue* queue, PlatformJobQueueCallbackFunc* callback, void* userData )
typedef PLATFORM_ADD_NEW_JOB(PlatformAddNewJobFunc);

#define PLATFORM_COMPLETE_ALL_JOBS(name) void name( PlatformJobQueue* queue )
typedef PLATFORM_COMPLETE_ALL_JOBS(PlatformCompleteAllJobsFunc);

#define PLATFORM_MAX_JOBQUEUE_JOBS 16768


// Providing a handle means we're updating the texture data instead of creating it anew
#define PLATFORM_ALLOCATE_OR_UPDATE_TEXTURE(name) void* name( void* data, int width, int height, bool filtered, void* optionalHandle )
typedef PLATFORM_ALLOCATE_OR_UPDATE_TEXTURE(PlatformAllocateOrUpdateTextureFunc);

#define PLATFORM_DEALLOCATE_TEXTURE(name) void name( void* handle )
typedef PLATFORM_DEALLOCATE_TEXTURE(PlatformDeallocateTextureFunc);


#define PLATFORM_LOG(name) void name( const char *fmt, ... )
typedef PLATFORM_LOG(PlatformLogFunc);


struct PlatformAPI
{
    DebugPlatformReadEntireFileFunc* DEBUGReadEntireFile;
    DebugPlatformFreeFileMemoryFunc* DEBUGFreeFileMemory;
    DebugPlatformWriteEntireFileFunc* DEBUGWriteEntireFile;
    DebugPlatformListAllAssetsFunc* DEBUGListAllAssets;
    DebugPlatformJoinPathsFunc* DEBUGJoinPaths;
    DebugPlatformGetParentPathFunc* DEBUGGetParentPath;
    DebugPlatformCurrentTimeMillis* DEBUGCurrentTimeMillis;

    PlatformAddNewJobFunc* AddNewJob;
    PlatformCompleteAllJobsFunc* CompleteAllJobs;
    PlatformJobQueue* hiPriorityQueue;
    //PlatformJobQueue* loPriorityQueue;
    // NOTE Includes the main thread! (0)
    i32 coreThreadsCount;

    PlatformAllocateOrUpdateTextureFunc* AllocateOrUpdateTexture;
    PlatformDeallocateTextureFunc* DeallocateTexture;

    PlatformLogFunc* Log;
};
extern PlatformAPI globalPlatform;


ASSERT_HANDLER(DefaultAssertHandler)
{
    LOG( "ASSERTION FAILED! :: '%s' (%s@%d)\n", msg, file, line );
}


#endif /* __PLATFORM_H__ */
