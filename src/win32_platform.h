#ifndef __WIN32_PLATFORM_H__
#define __WIN32_PLATFORM_H__ 


struct Win32OffscreenBuffer
{
    BITMAPINFO bitmapInfo;
    void *memory;
    int width;
    int height;
    int bytesPerPixel;
};

struct Win32WindowDimension
{
    int width;
    int height;
};

struct Win32AudioOutput
{
    u32 samplingRate;
    u32 bytesPerFrame;  
    u32 bufferSizeFrames;
    u16 systemLatencyFrames;
    u32 runningFrameCount;
};

struct Win32GameCode
{
    HMODULE gameCodeDLL;
    FILETIME lastDLLWriteTime;

    GameSetupAfterReloadFunc *SetupAfterReload;
    GameUpdateAndRenderFunc *UpdateAndRender;
    GameLogCallbackFunc *LogCallback;

    bool isValid;
};

struct Win32ReplayBuffer
{
    HANDLE fileHandle;
    HANDLE memoryMap;
    char filename[MAX_PATH];
    void *memoryBlock;
};

#define MAX_REPLAY_BUFFERS 3
struct Win32State
{
    HWND mainWindow;
    char exeFilePath[MAX_PATH];
    char currentDirectory[MAX_PATH];

    Win32GameCode gameCode;

    void *gameMemoryBlock;
    u64 gameMemorySize;
    Win32ReplayBuffer replayBuffers[MAX_REPLAY_BUFFERS];

    u32 inputRecordingIndex;
    HANDLE recordingHandle;
    u32 inputPlaybackIndex;
    HANDLE playbackHandle;

    HANDLE shadersDirHandle;
    OVERLAPPED shadersOverlapped;
    u8 shadersNotifyBuffer[8192];
};

#endif /* __WIN32_PLATFORM_H__ */
