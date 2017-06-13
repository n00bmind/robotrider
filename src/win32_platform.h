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
    int samplesPerSecond;
    int bytesPerSample;  
    int secondaryBufferSize;
    u16 latencySamples;
    u32 runningSampleIndex;
};

#endif /* __WIN32_PLATFORM_H__ */
