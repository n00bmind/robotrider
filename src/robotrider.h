#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#include <stdint.h>
#include <math.h>

#include "platform.h"
#include "math.h"



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
    b32 initialized;

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

    PlatformAPI platformAPI;
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

struct MemoryArena
{
    u8 *base;
    mem_idx size;
    mem_idx used;
};

struct FlyingDude
{
    v3 vertices[3];
    u32 indices[3];
    u32 VAO;
};

struct CubeThing
{
    v3 vertices[4];
    u32 indices[6];
    u32 VAO;

    v3 P;
    //m4 transformM;
};

struct World
{
    CubeThing *cubes;
};

struct GameState
{
    MemoryArena worldArena;
    World *world;

    u32 playerX;
    u32 playerY;
};



#endif /* __ROBOTRIDER_H__ */
