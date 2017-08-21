#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#include <string.h>
#include <stdint.h>
#include <math.h>

#include "platform.h"
#include "memory.h"
#include "math.h"
#include "renderer.h"


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

    m4 mCamera;

    RenderBuffer renderBuffer;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;

    RenderEntryTexturedTris *currentTris;
};

inline GameRenderCommands
InitRenderCommands( u8 *renderBuffer, u32 renderBufferSize,
                    TexturedVertex *vertexBuffer, u32 vertexBufferSize,
                    u32 *indexBuffer, u32 indexBufferSize )
{
    GameRenderCommands result;

    result.mCamera = Identity();
    result.renderBuffer.base = renderBuffer;
    result.renderBuffer.size = 0;
    result.renderBuffer.maxSize = renderBufferSize;
    result.vertexBuffer.base = vertexBuffer;
    result.vertexBuffer.size = 0;
    result.vertexBuffer.maxSize = vertexBufferSize;
    result.indexBuffer.base = indexBuffer;
    result.indexBuffer.size = 0;
    result.indexBuffer.maxSize = indexBufferSize;

    return result;
}

inline void
ResetRenderCommands( GameRenderCommands &commands )
{
    commands.renderBuffer.size = 0;
    commands.vertexBuffer.size = 0;
    commands.indexBuffer.size = 0;
}

struct GameAudioBuffer
{
    // TODO Remove this
    u32 samplesPerSecond;
    u32 frameCount;         // Audio frames to output
    u16 channelCount;       // Channels per frame
    // TODO Convert this to a format that is independent of final bitdepth (32bit-float?)
    // (even off-the-shelf audio mixers support this natively, it seems)
    i16 *samples;
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
    b32 executableReloaded;
    r32 secondsElapsed;

    GameControllerInput _controllers[5];

    // This is normal desktop mouse data (with standard system balllistics applied)
    i32 mouseX, mouseY, mouseZ;
    GameButtonState mouseButtons[5];
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
               GameRenderCommands &renderCommands, GameAudioBuffer *audioBuffer )
#endif
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunc);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{ }



//
// Other stuff
//

struct FlyingDude
{
    v3 vertices[3];
    u32 indices[3];

    m4 mTransform;
    u32 renderHandle;
};

struct CubeThing
{
    v3 vertices[4];
    u32 indices[6];

    m4 mTransform;
    u32 renderHandle;
};

struct GameState
{
    MemoryArena gameArena;

    v3 pPlayer;
    r32 playerPitch;
    r32 playerYaw;
};

struct TransientState
{
    b32 isInitialized;
    MemoryArena transientArena;

    RenderBuffer *renderBuffer;

    FlyingDude *dude;

    CubeThing *cubes;
    u32 cubeCount;
};


#endif /* __ROBOTRIDER_H__ */
