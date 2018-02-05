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
