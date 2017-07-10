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

#if DEBUG
#define NOT_IMPLEMENTED ASSERT(!"NotImplemented")
#else
#define NOT_IMPLEMENTED NotImplemented!!!
#endif

#define INVALID_CODE_PATH ASSERT("!InvalidCodePath")
#define INVALID_DEFAULT_CASE default: { INVALID_CODE_PATH; } break


#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES(value)*1024)


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

#include "math.h"


enum class Renderer
{
    OpenGL,
    // TODO OpenGLES,
    // Software?
};


//
// Services that the platform layer provides to the game
//


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

struct GameRenderCommands
{
    u16 width;
    u16 height;

    // TODO 
    void *renderEntries;
};

struct GameAudioBuffer
{
    // TODO Remove this
    u32 samplesPerSecond;
    u32 frameCount;         // Audio frames to output
    u16 channelCount;       // Channels per frame
    // TODO Convert this to a format that is independent of final bitdepth (32bit-float?)
    // (even off-the-shelf audio mixers support this natively, it seems)
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

    GameButtonState mouseButtons[5];
    s32 mouseX, mouseY, mouseZ;
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


#ifndef GAME_UPDATE_AND_RENDER
#define GAME_UPDATE_AND_RENDER(name) \
    void name( GameMemory *memory, GameInput *input, \
               GameRenderCommands *renderCommands, GameAudioBuffer *audioBuffer )
#endif
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunc);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{ }



//
// Other stuff
//

struct GameState
{
    int blueOffset;
    int greenOffset;
    int toneHz;
    r32 tSine;

    u32 playerX;
    u32 playerY;
};



#endif /* __ROBOTRIDER_H__ */
