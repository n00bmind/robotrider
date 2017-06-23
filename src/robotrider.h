#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#include <stdint.h>
#include <math.h>

#define global static
#define internal static
#define local_persistent static

#if DEBUG
#define ASSERT(expression) if( !(expression) ) { *(int *)0 = 0; }
#else
#define ASSERT(expression)
#endif

#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES(value)*1024)

#define PI32 3.141592653589f

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;
typedef float r32;
typedef double r64;



inline u32 SafeTruncU64( u64 value )
{
    ASSERT( value <= 0xFFFFFFFF );
    u32 result = (u32)value;
    return result;
}

//
// Services that the platform layer provides to the game
//
#if DEBUG

struct DEBUGReadFileResult
{
    u32 contentSize;
    void *contents;
};
#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name( char *filename )
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(DebugPlatformReadEntireFileFunc);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name( void *memory )
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(DebugPlatformFreeFileMemoryFunc);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) b32 name( char*filename, u32 memorySize, void *memory )
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DebugPlatformWriteEntireFileFunc);

#endif


//
// Game entry points & data types for the platform layer
//
struct GameOffscreenBuffer
{
    void *memory;
    int width;
    int height;
    int bytesPerPixel;
};

struct GameAudioBuffer
{
    // TODO Remove this
    u32 samplesPerSecond;
    u32 frameCount;         // Audio frames to output
    u16 channelCount;       // Channels per frame
    // TODO Convert this to a format that is independent of final bitdepth (32bit-float?)
    // (some audio mixers even support this natively, it seems)
    s16 *samples;
};

struct GameStickState
{
    // -1.0 to 1.0 range (negative is left/down, positive is up/right)
    r32 startX;
    r32 startY;
    r32 avgX;
    r32 avgY;
    r32 endX;
    r32 endY;
};

struct GameButtonState
{
    int halfTransitionCount;
    b32 endedDown;
};

struct GameControllerInput
{
    b32 isConnected;
    b32 isAnalog;
    
    GameStickState leftStick;
    GameStickState rightStick;

    union
    {
        struct
        {
            GameButtonState dUp;
            GameButtonState dDown;
            GameButtonState dLeft;
            GameButtonState dRight;

            GameButtonState aButton;
            GameButtonState bButton;
            GameButtonState xButton;
            GameButtonState yButton;

            GameButtonState leftThumb;
            GameButtonState rightThumb;
            GameButtonState leftShoulder;
            GameButtonState rightShoulder;

            GameButtonState start;
            GameButtonState back;
        };
        GameButtonState buttons[14];
    };
};

struct GameInput
{
    r32 secondsElapsed;
    GameControllerInput _controllers[5];
};

inline GameControllerInput *
GetController( GameInput *input, int controllerIndex )
{
    ASSERT( controllerIndex < ARRAYCOUNT( input->_controllers ) );
    GameControllerInput *result = &input->_controllers[controllerIndex];
    return result;
}

struct GameMemory
{
    b32 isInitialized;

    u64 permanentStorageSize;
    void *permanentStorage;     // NOTE Required to be cleared to zero at startup

    u64 transientStorageSize;
    void *transientStorage;     // NOTE Required to be cleared to zero at startup

    DebugPlatformReadEntireFileFunc *DEBUGPlatformReadEntireFile;
    DebugPlatformFreeFileMemoryFunc *DEBUGPlatformFreeFileMemory;
    DebugPlatformWriteEntireFileFunc *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name) \
    void name( GameMemory *memory, GameInput *input, \
               GameOffscreenBuffer *videoBuffer, GameAudioBuffer *audioBuffer, b32 beep )
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunc);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}



//
// Other stuff
//

struct GameState
{
    int blueOffset;
    int greenOffset;
    int toneHz;
};



#endif /* __ROBOTRIDER_H__ */
