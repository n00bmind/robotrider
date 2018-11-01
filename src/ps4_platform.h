#ifndef __PS4_PLATFORM_H__
#define __PS4_PLATFORM_H__ 

#define MAX_PATH 1024

struct PS4MemoryBlock
{
    void* address;
    off_t physicalOffset;

    sz size;
};

struct PlatformJobQueueJob
{
    PlatformJobQueueCallbackFunc* callback;
    void* userData;
};

struct PlatformJobQueue
{
    void* semaphore;
    volatile u32 nextJobToRead;
    volatile u32 nextJobToWrite;

    volatile u32 completionCount;
    volatile u32 completionTarget;

    PlatformJobQueueJob jobs[PLATFORM_MAX_JOBQUEUE_JOBS];
};

struct PS4WorkerThreadContext
{
    u32 threadIndex;
    PlatformJobQueue* queue;
};

struct PS4GameCode
{
    SceKernelModule gameCodePrx;
    timespec lastPrxWriteTime;

    GameSetupAfterReloadFunc* SetupAfterReload;
    GameUpdateAndRenderFunc* UpdateAndRender;
    GameLogCallbackFunc* LogCallback;
    DebugGameFrameEndFunc* DebugFrameEnd;

    bool isValid;
};

enum class PS4Path
{
    Binary,
    Asset,
};

struct PS4ControllerInfo
{
    i32 handle;
    ScePadControllerInformation pad;
};

struct PS4RendererState;

#define MAX_REPLAY_BUFFERS 3
struct PS4State
{
    char binariesPath[MAX_PATH];
    char assetDataPath[MAX_PATH];

    PS4GameCode gameCode;
    PS4ControllerInfo controllerInfos[ARRAYCOUNT(GameInput::_controllers)];     // Map 1-to-1 to controllers
    PS4RendererState* renderer;

    PS4MemoryBlock memoryBlocks[64];
    u32 nextFreeMemoryBlock;

    void *gameMemoryBlock;
    sz gameMemorySize;
    void* /*PS4ReplayBuffer*/ replayBuffers[MAX_REPLAY_BUFFERS];

    bool running;

    u32 inputRecordingIndex;
    void* recordingHandle;
    u32 inputPlaybackIndex;
    void* playbackHandle;

    PlatformJobQueue hiPriorityQueue;
};



internal void*
PS4AllocAndMap( void* address, sz size, int memoryType, sz alignment = 0,
                int protection = SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW );
internal void
PS4Free( void* address );

internal void
PS4BuildAbsolutePath( const char* filename, PS4Path pathType, char *outPath );


#endif /* __PS4_PLATFORM_H__ */
