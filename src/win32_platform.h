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

struct PlatformJobQueueJob
{
    PlatformJobQueueCallbackFunc* callback;
    void* userData;
};

struct PlatformJobQueue
{
    HANDLE semaphore;
    volatile u32 nextJobToRead;
    volatile u32 nextJobToWrite;

    volatile u32 completionCount;
    volatile u32 completionTarget;

    PlatformJobQueueJob jobs[PLATFORM_MAX_JOBQUEUE_JOBS];
};

struct Win32WorkerThreadContext
{
    u32 threadIndex;
    PlatformJobQueue* queue;
};

struct Win32GameCode
{
    HMODULE gameCodeDLL;
    FILETIME lastDLLWriteTime;

    GameSetupAfterReloadFunc* SetupAfterReload;
    GameUpdateAndRenderFunc* UpdateAndRender;
    GameLogCallbackFunc* LogCallback;
    DebugGameFrameEndFunc* DebugFrameEnd;

    bool isValid;
};

struct Win32ReplayBuffer
{
    HANDLE fileHandle;
    HANDLE memoryMap;
    char filename[PLATFORM_PATH_MAX];
    void *memoryBlock;
};

struct Win32AssetUpdateListener
{
    const char* relativePath;
    const char* extension;
    DEBUGGameAssetLoadedCallbackFunc* callback;
    HANDLE dirHandle;
    OVERLAPPED overlapped;
};

#define MAX_REPLAY_BUFFERS 3
struct Win32State
{
    HWND mainWindow;

    char currentDirectory[PLATFORM_PATH_MAX];
    char exeFilePath[PLATFORM_PATH_MAX];
    char sourceDLLPath[PLATFORM_PATH_MAX];
    char tempDLLPath[PLATFORM_PATH_MAX];
    char assetDataPath[PLATFORM_PATH_MAX];

    Win32GameCode gameCode;
    Renderer renderer;

    void *gameMemoryBlock;
    u64 gameMemorySize;
    Win32ReplayBuffer replayBuffers[MAX_REPLAY_BUFFERS];

    u32 inputRecordingIndex;
    HANDLE recordingHandle;
    u32 inputPlaybackIndex;
    HANDLE playbackHandle;

    // NOTE I operate under the assumption that this won't be filled until our call to GetOverlappedResult return true,
    // hence we should be able to just use one shared buffer for all our installed listeners. I could be wrong though!
    u8 assetsNotifyBuffer[1024];
    Win32AssetUpdateListener *assetListeners;
    u32 assetListenerCount;

    PlatformJobQueue hiPriorityQueue;
};

#endif /* __WIN32_PLATFORM_H__ */
