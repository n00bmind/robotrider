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

#include <ctype.h>
#include <stdarg.h>


#include "game.h"

#include "memory.h"
#include "data_types.h"
#include "meshgen.h"
#include "world.h"
#include "robotrider.h"
#include "asset_loaders.h"

#include "renderer.cpp"

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
// TODO Remove!
#include "imgui/imgui_demo.cpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) ASSERT(x)
// TODO Define STBI_MALLOC, STBI_REALLOC, STBI_FREE
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#include "stb/stb_image.h"

#include "ui.cpp"
#include "console.cpp"
#include "asset_loaders.cpp"
#include "meshgen.cpp"
#include "world.cpp"
#include "editor.cpp"



PlatformAPI globalPlatform;
internal GameConsole *gameConsole;

#if DEBUG
DebugGameStats* DEBUGglobalStats;
#endif


LIB_EXPORT
GAME_LOG_CALLBACK(GameLogCallback)
{
    ConsoleLog( gameConsole, msg );
}

// TODO Remove
LIB_EXPORT
GAME_SETUP_AFTER_RELOAD(GameSetupAfterReload)
{
    //ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    //GameState *gameState = (GameState *)memory->permanentStorage;
    //gameConsole = &gameState->gameConsole;

    //// Re-set platform's ImGui context
    //ImGui::SetCurrentContext( gameState->imGuiContext );

    // Do this in a real test suite
    TestDataTypes();
}

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    TIMED_BLOCK(GameUpdateAndRender);

    globalPlatform = *memory->platformAPI;
#if DEBUG
    DEBUGglobalStats = memory->DEBUGgameStats;
#endif

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState *gameState = (GameState *)memory->permanentStorage;
    ASSERT( sizeof(TransientState) <= memory->transientStorageSize );
    TransientState *tranState = (TransientState *)memory->transientStorage;

    if( input->executableReloaded )
    {
        // TODO Check if these are all ok here so that we can remove GAME_SETUP_AFTER_RELOAD
        gameConsole = &gameState->gameConsole;
        // Re-set platform's ImGui context
        ImGui::SetCurrentContext( memory->imGuiContext );
    }

    // Init transient arena
    if( !tranState->isInitialized )
    {
        InitializeArena( &tranState->transientArena,
                         (u8 *)memory->transientStorage + sizeof(TransientState),
                         memory->transientStorageSize - sizeof(TransientState));

        tranState->isInitialized = true;
    }

    TemporaryMemory tempMemory = BeginTemporaryMemory( &tranState->transientArena );

    // Init game arena & world state
    if( !memory->isInitialized )
    {
        InitializeArena( &gameState->worldArena,
                         (u8 *)memory->permanentStorage + sizeof(GameState),
                         memory->permanentStorageSize - sizeof(GameState) );

        gameState->world = PUSH_STRUCT( &gameState->worldArena, World );
        InitWorld( gameState->world, &gameState->worldArena, &tranState->transientArena );

        memory->isInitialized = true;
    }

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    {
        GameInput* gameInput = input;

#if DEBUG
        GameInput dummyInput;
        if( memory->DEBUGglobalEditing )
        {
            dummyInput =
            {
                input->executableReloaded,
                input->frameElapsedSeconds,
                input->totalElapsedSeconds,
                input->frameCounter,
            };
            gameInput = &dummyInput;
        }
#endif

        PushClear( { 0.95f, 0.95f, 0.95f, 1.0f }, renderCommands );
        UpdateAndRenderWorld( gameInput, gameState, renderCommands );
    }

#if DEBUG
    float fps = ImGui::GetIO().Framerate; //1.f / input->frameElapsedSeconds;
    float frameTime = 1000.f / fps;
    char statsText[1024];
    snprintf( statsText, ARRAYCOUNT(statsText),
              "Frame ms.: %.3f (%.1f FPS)   DrawCalls %u   Primitives %u   Vertices %u",
              frameTime, fps, DEBUGglobalStats->totalDrawCalls,
              DEBUGglobalStats->totalPrimitiveCount, DEBUGglobalStats->totalVertexCount );

    if( memory->DEBUGglobalEditing )
    {
        UpdateAndRenderEditor( input, memory, renderCommands, statsText );
    }
    else if( memory->DEBUGglobalDebugging )
    {
        DrawConsole( &gameState->gameConsole, width, height, statsText );
        ImGui::SetNextWindowPos( ImVec2( width - 500.f, height - 300.f ) );
        ImGui::ShowUserGuide();
    }
    else
        DrawStats( width, height, statsText );
#endif

    EndTemporaryMemory( tempMemory );

    CheckArena( &gameState->worldArena );
    CheckArena( &tranState->transientArena );
}

