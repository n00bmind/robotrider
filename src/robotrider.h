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
InitRenderCommands( u8 *renderBuffer, u32 renderBufferMaxSize,
                    TexturedVertex *vertexBuffer, u32 vertexBufferMaxCount,
                    u32 *indexBuffer, u32 indexBufferMaxCount )
{
    GameRenderCommands result;

    result.mCamera = Identity();
    result.renderBuffer.base = renderBuffer;
    result.renderBuffer.size = 0;
    result.renderBuffer.maxSize = renderBufferMaxSize;
    result.vertexBuffer.base = vertexBuffer;
    result.vertexBuffer.count = 0;
    result.vertexBuffer.maxCount = vertexBufferMaxCount;
    result.indexBuffer.base = indexBuffer;
    result.indexBuffer.count = 0;
    result.indexBuffer.maxCount = indexBufferMaxCount;

    return result;
}

inline void
ResetRenderCommands( GameRenderCommands &commands )
{
    commands.renderBuffer.size = 0;
    commands.vertexBuffer.count = 0;
    commands.indexBuffer.count = 0;
    commands.currentTris = 0;
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
    // TODO Is this one really necessary? Review how we're using it, if at all
    int halfTransitionCount;
    bool endedDown;
};

struct GameControllerInput
{
    bool isConnected;
    bool isAnalog;
    
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
    bool executableReloaded;
    r32 frameElapsedSeconds;

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
    bool isInitialized;

    u64 permanentStorageSize;
    void *permanentStorage;     // NOTE Required to be cleared to zero at startup

    u64 transientStorageSize;
    void *transientStorage;     // NOTE Required to be cleared to zero at startup

    PlatformAPI platformAPI;
};

enum class ConsoleEntryType
{
    LogOutput,
    History,
    CommandOutput,
};

struct ConsoleEntry
{
    // FIXME This is absurd and we should allocate this as needed in the gameArena
    char text[1024];
    ConsoleEntryType type;
};

struct GameConsole
{
    ConsoleEntry entries[4096];
    char inputBuffer[1024];

    bool scrollToBottom;
};


struct ImGuiContext;

struct GameState
{
    MemoryArena gameArena;
    ImGuiContext* imGuiContext;

    GameConsole gameConsole;

#if DEBUG
    bool DEBUGglobalDebugging;
    bool DEBUGglobalEditing;
#endif

    v3 pPlayer;
    r32 playerPitch;
    r32 playerYaw;
};


#ifndef GAME_SETUP_AFTER_RELOAD
#define GAME_SETUP_AFTER_RELOAD(name) \
    void name( GameState *gameState )
#endif
typedef GAME_SETUP_AFTER_RELOAD(GameSetupAfterReloadFunc);

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

struct TransientState
{
    bool isInitialized;
    MemoryArena transientArena;

    RenderBuffer *renderBuffer;

    FlyingDude *dude;

    CubeThing *cubes;
    u32 cubeCount;
};


#endif /* __ROBOTRIDER_H__ */
