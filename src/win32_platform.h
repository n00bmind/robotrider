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

#endif /* __WIN32_PLATFORM_H__ */
