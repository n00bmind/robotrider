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
#ifndef __GAME_H__
#define __GAME_H__ 

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>


#include "common.h"
#include "intrinsics.h"
#include "memory.h"
#include "data_types.h"
#include "math.h"
#include "math_types.h"
#include "platform.h"
#include "debugstats.h"
#include "renderer.h"



//
// Game entry points & data types for the platform layer
// (should be as separate as possible from both the platform and the game 'implementation')
//

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
    
    GameStickState leftStick;
    GameStickState rightStick;

    // TODO Left/right triggers (Win)
    r32 leftTriggerValue;               // 0 to 1 range
    r32 rightTriggerValue;

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
            GameButtonState options;
            GameButtonState back;
        };
        GameButtonState buttons[14];
    };
};

// OPTIONAL: some platforms may not provide this
struct EditorInput
{
    // This is normal desktop mouse data (with standard system ballistics applied)
    i32 mouseX, mouseY, mouseZ;
    GameButtonState mouseButtons[5];

    // TODO Platform-agnostic way to pass raw keypress data to the game?
    union
    {
        struct
        {
            GameButtonState openEditor; // TODO
            GameButtonState closeEditor; // TODO
            GameButtonState triggerEditor; // TODO

            GameButtonState nextStep;
        };
        GameButtonState buttons[4];
    };
};

struct GameInput
{
    bool executableReloaded;
    r32 frameElapsedSeconds;
    r32 totalElapsedSeconds;
    u32 frameCounter;

    GameControllerInput _controllers[5];

    bool hasEditorInput;
    EditorInput editor;
};

#define PLATFORM_KEYMOUSE_CONTROLLER_SLOT 0

inline GameControllerInput *
GetController( GameInput *input, u32 controllerIndex )
{
    ASSERT( controllerIndex < ARRAYCOUNT( input->_controllers ) );
    GameControllerInput *result = &input->_controllers[controllerIndex];
    return result;
}


struct ImGuiContext;

struct GameMemory
{
    bool isInitialized;

    u64 permanentStorageSize;
    void *permanentStorage;         // NOTE Required to be cleared to zero at startup

    u64 transientStorageSize;
    void *transientStorage;         // NOTE Required to be cleared to zero at startup

#if !RELEASE
    u64 debugStorageSize;
    void *debugStorage;             // NOTE Required to be cleared to zero at startup

    bool DEBUGglobalDebugging;
    bool DEBUGglobalEditing;
#endif

    PlatformAPI* platformAPI;
    ImGuiContext* imGuiContext;
};



#define GAME_SETUP_AFTER_RELOAD(name) void name( GameMemory *memory )
typedef GAME_SETUP_AFTER_RELOAD(GameSetupAfterReloadFunc);

#define GAME_UPDATE_AND_RENDER(name) \
    void name( GameMemory *memory, GameInput *input, RenderCommands *renderCommands, GameAudioBuffer *audioBuffer )
typedef GAME_UPDATE_AND_RENDER(GameUpdateAndRenderFunc);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{ }

#define DEBUG_GAME_ASSET_LOADED_CALLBACK(name) void name( const char *filename, DEBUGReadFileResult readFile )
typedef DEBUG_GAME_ASSET_LOADED_CALLBACK(DEBUGGameAssetLoadedCallbackFunc);

#define GAME_LOG_CALLBACK(name) void name( const char *msg )
typedef GAME_LOG_CALLBACK(GameLogCallbackFunc);

struct DebugFrameInfo
{
    r32 inputProcessedSeconds;
    r32 gameUpdatedSeconds;
    r32 audioUpdatedSeconds;
    r32 endOfFrameSeconds;
};

#define DEBUG_GAME_FRAME_END(name) void name( GameMemory* memory, const DebugFrameInfo* frameInfo )
typedef DEBUG_GAME_FRAME_END(DebugGameFrameEndFunc);

#endif /* __GAME_H__ */
