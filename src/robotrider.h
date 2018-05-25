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

#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

#include "platform.h"
#include "memory.h"
#include "math.h"
#include "data_types.h"
#include "renderer.h"
#include "meshgen.h"
#include "world.h"


//
// Game entry points & data types for the platform layer
// (should probably try to separate it as much as possible to minimize what the
// platform side needs to include)
//

// TODO Move to renderer.h?
struct GameRenderCommands
{
    u16 width;
    u16 height;

    Camera camera;

    RenderBuffer renderBuffer;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;

    RenderEntryTexturedTris *currentTris;
    RenderEntryLines *currentLines;
};

inline GameRenderCommands
InitRenderCommands( u8 *renderBuffer, u32 renderBufferMaxSize,
                    TexturedVertex *vertexBuffer, u32 vertexBufferMaxCount,
                    u32 *indexBuffer, u32 indexBufferMaxCount )
{
    GameRenderCommands result;

    result.renderBuffer.base = renderBuffer;
    result.renderBuffer.size = 0;
    result.renderBuffer.maxSize = renderBufferMaxSize;
    result.vertexBuffer.base = vertexBuffer;
    result.vertexBuffer.count = 0;
    result.vertexBuffer.maxCount = vertexBufferMaxCount;
    result.indexBuffer.base = indexBuffer;
    result.indexBuffer.count = 0;
    result.indexBuffer.maxCount = indexBufferMaxCount;

    result.currentTris = nullptr;
    result.currentLines = nullptr;

    return result;
}

inline void
ResetRenderCommands( GameRenderCommands *commands )
{
    commands->renderBuffer.size = 0;
    commands->vertexBuffer.count = 0;
    commands->indexBuffer.count = 0;
    commands->currentTris = 0;
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
    r32 totalElapsedSeconds;
    u32 frameCounter;

    GameControllerInput _controllers[5];

    // This is normal desktop mouse data (with standard system ballistics applied)
    i32 mouseX, mouseY, mouseZ;
    GameButtonState mouseButtons[5];
    // TODO Platform-agnostic way to pass raw keypress data to the game
};

inline GameControllerInput *
GetController( GameInput *input, u32 controllerIndex )
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

    PlatformAPI *platformAPI;
};


#ifndef GAME_SETUP_AFTER_RELOAD
#define GAME_SETUP_AFTER_RELOAD(name) \
    void name( GameMemory *memory )
#endif
typedef GAME_SETUP_AFTER_RELOAD(GameSetupAfterReloadFunc);

#ifndef GAME_UPDATE_AND_RENDER
#define GAME_UPDATE_AND_RENDER(name) \
    void name( GameMemory *memory, GameInput *input, \
               GameRenderCommands *renderCommands, GameAudioBuffer *audioBuffer )
#endif
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunc);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{ }

#ifndef GAME_ASSET_LOADED_CALLBACK
#define GAME_ASSET_LOADED_CALLBACK(name) \
    void name( const char *assetName, const u8 *contents, u32 len )
#endif
typedef GAME_ASSET_LOADED_CALLBACK(GameAssetLoadedCallbackFunc);

#ifndef GAME_LOG_CALLBACK
#define GAME_LOG_CALLBACK(name) \
    void name( const char *msg )
#endif
typedef GAME_LOG_CALLBACK(GameLogCallbackFunc);


struct GameAssetMapping
{
    const char *relativePath;
    const char *extension;
    GameAssetLoadedCallbackFunc *callback;
};



//
// Other stuff
//

enum class ConsoleEntryType
{
    Empty = 0,
    LogOutput,
    History,
    CommandOutput,
};

#define CONSOLE_LINE_MAXLEN 1024

struct ConsoleEntry
{
    char text[CONSOLE_LINE_MAXLEN];
    ConsoleEntryType type;
};

struct GameConsole
{
    // FIXME This is absurd and we should allocate this as needed in the gameArena
    ConsoleEntry entries[4096];
    char inputBuffer[CONSOLE_LINE_MAXLEN];

    u32 entryCount;
    u32 nextEntryIndex;
    bool scrollToBottom;
};

struct EditorState
{
    v3 pCamera;
    r32 camPitch;
    r32 camYaw;
};

struct ImGuiContext;

struct GameState
{
    ImGuiContext* imGuiContext;
    GameConsole gameConsole;

    MemoryArena worldArena;
    World *world;

#if DEBUG
    bool DEBUGglobalDebugging;
    bool DEBUGglobalEditing;

    EditorState DEBUGeditorState;
#endif

};

struct TransientState
{
    bool isInitialized;
    MemoryArena transientArena;
};


#endif /* __ROBOTRIDER_H__ */
