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

struct Win32SoundOutput
{
    u32 samplesPerSecond;
    u32 bytesPerSample;  
    u32 secondaryBufferSize;
    u32 latencySamples;
    u32 runningSampleIndex;
};

#endif /* __WIN32_PLATFORM_H__ */
