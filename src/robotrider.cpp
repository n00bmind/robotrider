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

#include "robotrider.h"
#include "data_types.h"
#include "renderer.cpp"

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
// TODO Remove!
#include "imgui/imgui_demo.cpp"

#include "ui.cpp"
#include "console.cpp"
#include "world.cpp"
#include "editor.cpp"


PlatformAPI globalPlatform;
internal GameConsole *gameConsole;

LIB_EXPORT
GAME_LOG_CALLBACK(GameLogCallback)
{
    ConsoleLog( gameConsole, msg );
}

LIB_EXPORT
GAME_SETUP_AFTER_RELOAD(GameSetupAfterReload)
{
    globalPlatform = *memory->platformAPI;

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState *gameState = (GameState *)memory->permanentStorage;
    gameConsole = &gameState->gameConsole;

    // Re-set platform's ImGui context
    ImGui::SetCurrentContext( gameState->imGuiContext );
}

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    GameState *gameState = (GameState *)memory->permanentStorage;

    // Init game arena & world state
    if( !memory->isInitialized )
    {
        InitializeArena( &gameState->gameArena,
                         (u8 *)memory->permanentStorage + sizeof(GameState),
                         memory->permanentStorageSize - sizeof(GameState) );

        InitWorld( gameState );

        memory->isInitialized = true;
    }

    // Init transient arena
    ASSERT( sizeof(TransientState) <= memory->transientStorageSize );
    TransientState *tranState = (TransientState *)memory->transientStorage;
    if( input->executableReloaded )
    {
        tranState->isInitialized = false;
    }
    if( !tranState->isInitialized )
    {
        InitializeArena( &tranState->transientArena,
                         (u8 *)memory->transientStorage + sizeof(TransientState),
                         memory->transientStorageSize - sizeof(TransientState));

        tranState->isInitialized = true;
    }

    BucketTest();

#if DEBUG
    if( gameState->DEBUGglobalEditing )
        UpdateAndRenderEditor( input, memory, renderCommands );
    else
#endif
    {
        //TemporaryMemory renderMemory = BeginTemporaryMemory( &tranState->transientArena );

#if DEBUG
        u16 width = renderCommands->width;
        u16 height = renderCommands->height;

        float fps = ImGui::GetIO().Framerate; //1.f / input->frameElapsedSeconds;
        char statsText[1024];
        snprintf( statsText, 1024, "FPS %.1f", fps );   // Seems to be crossplatform, so it's good enough for now
        //LOG( "Elapsed %.1f seconds", input->gameElapsedSeconds );

        if( gameState->DEBUGglobalDebugging )
        {
            DrawConsole( &gameState->gameConsole, width, height, statsText );
            ImGui::SetNextWindowPos( ImVec2( width - 500.f, height - 300.f ) );
            ImGui::ShowUserGuide();
        }
        else
            DrawStats( width, height, statsText );
#endif

        // Update player based on input
        {
            float dt = input->frameElapsedSeconds;
            GameControllerInput *input0 = GetController( input, 0 );

            FlyingDude *playerDude = gameState->playerDude;
            v3 pPlayer = gameState->pPlayer;
            v3 vPlayerDelta = {};

            if( input0->dLeft.endedDown )
            {
                vPlayerDelta.x -= 3.f * dt;
            }
            if( input0->dRight.endedDown )
            {
                vPlayerDelta.x += 3.f * dt;
            }
            if( input0->dUp.endedDown )
            {
                vPlayerDelta.y += 3.f * dt;
            }
            if( input0->dDown.endedDown )
            {
                vPlayerDelta.y -= 3.f * dt;
            }

            if( input0->rightStick.avgX || input0->rightStick.avgY )
            {
                gameState->playerPitch += -input0->rightStick.avgY / 15.f * dt;
                gameState->playerYaw += -input0->rightStick.avgX / 15.f * dt; 
            }

            m4 mPlayerRot = ZRotation( gameState->playerYaw ) * XRotation( gameState->playerPitch );
            pPlayer = pPlayer + mPlayerRot * vPlayerDelta;
            playerDude->mTransform = RotPos( mPlayerRot, pPlayer );

            gameState->pPlayer = pPlayer;
        }

        PushClear( { 0.95f, 0.95f, 0.95f, 1.0f }, renderCommands );
        UpdateAndRenderWorld( gameState, renderCommands );

        {
            FlyingDude *playerDude = gameState->playerDude;
            // Create a chasing camera
            // TODO Use a PID controller
            v3 pCam = playerDude->mTransform * V3( 0, -2, 1 );
            v3 pLookAt = playerDude->mTransform * V3( 0, 1, 0 );
            v3 vUp = GetColumn( playerDude->mTransform, 2 ).xyz;
            renderCommands->camera.mTransform = CameraLookAt( pCam, pLookAt, vUp );
        }

        //EndTemporaryMemory( renderMemory );
    }

    CheckArena( &gameState->gameArena );
    CheckArena( &tranState->transientArena );
}

