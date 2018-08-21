#ifndef __PS4_PLATFORM_H__
#define __PS4_PLATFORM_H__ 

#define MAX_PATH 1024


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

#define MAX_REPLAY_BUFFERS 3
struct PS4State
{
    char currentDirectory[MAX_PATH];
    char elfFilePath[MAX_PATH];
    char sourceDLLPath[MAX_PATH];
    char tempDLLPath[MAX_PATH];
    char assetDataPath[MAX_PATH];

    PS4GameCode gameCode;
    Renderer renderer;

    void *gameMemoryBlock;
    u64 gameMemorySize;
    void* /*PS4ReplayBuffer*/ replayBuffers[MAX_REPLAY_BUFFERS];

    bool running;

    u32 inputRecordingIndex;
    void* recordingHandle;
    u32 inputPlaybackIndex;
    void* playbackHandle;

    PlatformJobQueue hiPriorityQueue;
};

#endif /* __PS4_PLATFORM_H__ */
