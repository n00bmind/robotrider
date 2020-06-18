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

#pragma warning( push )

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

#pragma warning( pop )

#include "common.h"
#include "intrinsics.h"
#include "memory.h"
#include "math_types.h"
#include "math_sdf.h"
#include "math.h"
#include "macro_madness.h"
#include "data_types.h"
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
    i32 frameCount;         // Audio frames to output
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

enum GameMouseButtons
{
    MouseButtonLeft = 0,
    MouseButtonMiddle = 1,
    MouseButtonRight = 2,
    MouseButton4 = 3,
    MouseButton5 = 4,
};

#pragma region USB HID Key Codes
// As specified in https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf Chapter 10
// NOTE Names are based on a US layout
enum GameKeyCodes
{
    KeyNone = 0,

    KeyA = 4,
    KeyB = 5,
    KeyC = 6,
    KeyD = 7,
    KeyE = 8,
    KeyF = 9,
    KeyG = 10,
    KeyH = 11,
    KeyI = 12,
    KeyJ = 13,
    KeyK = 14,
    KeyL = 15,
    KeyM = 16,
    KeyN = 17,
    KeyO = 18,
    KeyP = 19,
    KeyQ = 20,
    KeyR = 21,
    KeyS = 22,
    KeyT = 23,
    KeyU = 24,
    KeyV = 25,
    KeyW = 26,
    KeyX = 27,
    KeyY = 28,
    KeyZ = 29,

    Key1 = 30,
    Key2 = 31,
    Key3 = 32,
    Key4 = 33,
    Key5 = 34,
    Key6 = 35,
    Key7 = 36,
    Key8 = 37,
    Key9 = 38,
    Key0 = 39,

    KeyEnter = 40,
    KeyEscape = 41,
    KeyBackspace = 42,
    KeyTab = 43,
    KeySpace = 44,
    KeyMinus = 45,
    KeyEquals = 46,
    KeyLeftBracket = 47,
    KeyRightBracket = 48,
    KeyBackslash = 49,
    KeySemicolon = 51,
    KeyQuote = 52,
    KeyGrave = 53,
    KeyComma = 54,
    KeyPeriod = 55,
    KeySlash = 56,
    KeyCapsLock = 57,

    KeyF1 = 58,
    KeyF2 = 59,
    KeyF3 = 60,
    KeyF4 = 61,
    KeyF5 = 62,
    KeyF6 = 63,
    KeyF7 = 64,
    KeyF8 = 65,
    KeyF9 = 66,
    KeyF10 = 67,
    KeyF11 = 68,
    KeyF12 = 69,

    KeyPrintScreen = 70,
    KeyScrollLock = 71,
    KeyPause = 72,
    KeyInsert = 73,
    KeyHome = 74,
    KeyPageUp = 75,
    KeyDelete = 76,
    KeyEnd = 77,
    KeyPageDown = 78,
    KeyRight = 79,
    KeyLeft = 80,
    KeyDown = 81,
    KeyUp = 82,

    KeyNumLock = 83,
    KeyKPDivide = 84,
    KeyKPMultiply = 85,
    KeyKPSubtract = 86,
    KeyKPAdd = 87,
    KeyKPEnter = 88,
    KeyKP1 = 89,
    KeyKP2 = 90,
    KeyKP3 = 91,
    KeyKP4 = 92,
    KeyKP5 = 93,
    KeyKP6 = 94,
    KeyKP7 = 95,
    KeyKP8 = 96,
    KeyKP9 = 97,
    KeyKP0 = 98,
    KeyKPPoint = 99,
    KeyNonUSBackslash = 100,
    KeyKPEquals = 103,

    KeyHelp = 117,
    KeyMenu = 118,
    KeyLeftControl = 224,
    KeyLeftShift = 225,
    KeyLeftAlt = 226,
    KeyLeftGUI = 227,
    KeyRightControl = 228,
    KeyRightShift = 229,
    KeyRightAlt = 230,
    KeyRightGUI = 231,

    KeyUnknown = 0xFF,
    KeyCOUNT
};
#pragma endregion USB HID Key Codes

// OPTIONAL: some platforms may not provide this
struct KeyMouseInput
{
    // This is normal desktop mouse data (with standard system ballistics applied)
    i32 mouseX, mouseY, mouseZ;
    // Raw mouse data as obtained from the platform
    i32 mouseRawXDelta, mouseRawYDelta;
    r32 mouseRawZDelta;
    GameButtonState mouseButtons[5];

    // Platform-agnostic key codes
    bool keysDown[KeyCOUNT];
};

struct GameInput
{
    bool gameCodeReloaded;
    r32 frameElapsedSeconds;
    r32 totalElapsedSeconds;
    u32 frameCounter;

    GameControllerInput _controllers[5];

    bool hasKeyMouseInput;
    KeyMouseInput keyMouse;
};

#define PLATFORM_KEYMOUSE_CONTROLLER_SLOT 0

inline GameControllerInput *
GetController( GameInput *input, int controllerIndex )
{
    ASSERT( controllerIndex < ARRAYCOUNT( input->_controllers ) );
    GameControllerInput *result = &input->_controllers[controllerIndex];
    return result;
}
inline const GameControllerInput&
GetController( const GameInput& input, int controllerIndex )
{
    ASSERT( controllerIndex < ARRAYCOUNT( input._controllers ) );
    return input._controllers[controllerIndex];
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

    // TODO Put these inside DebugState
    // (and make the platform not modify them directly, just pass the relevant input so the game can do that!)
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
