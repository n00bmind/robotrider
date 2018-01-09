#include "robotrider.h"
#include "renderer.cpp"
#include "ui.cpp"
#include "console.cpp"
#include "world.cpp"
#include "editor.cpp"


PlatformAPI globalPlatform;

internal GameConsole *gameConsole;
GAME_LOG_CALLBACK(GameLogCallback)
{
    ConsoleLog( gameConsole, msg );
}

LIB_EXPORT
GAME_SETUP_AFTER_RELOAD(GameSetupAfterReload)
{
    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState *gameState = (GameState *)memory->permanentStorage;

    // Copy all logs to the game's console
    gameConsole = &gameState->gameConsole;
    memory->platformAPI->LogCallback = GameLogCallback;

    globalPlatform = *memory->platformAPI;
    // Make sure we never use our own copy (this is focking awful)
    globalPlatform.LogCallback = NULL;

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

        PushClear( renderCommands, { 0.95f, 0.95f, 0.95f, 1.0f } );
        UpdateAndRenderWorld( gameState, renderCommands );

        {
            FlyingDude *playerDude = gameState->playerDude;
            // Create a chasing camera
            // TODO Use a PID controller
            v3 pCam = playerDude->mTransform * V3( 0, -2, 1 );
            v3 pLookAt = playerDude->mTransform * V3( 0, 1, 0 );
            v3 vUp = GetColumn( playerDude->mTransform, 2 ).xyz;
            renderCommands->mCamera = CameraLookAt( pCam, pLookAt, vUp );
        }

        //EndTemporaryMemory( renderMemory );
    }

    CheckArena( &gameState->gameArena );
    CheckArena( &tranState->transientArena );
}


#if 0
internal void
DEBUGRenderWeirdGradient( GameOffscreenBuffer *buffer, int xOffset, int yOffset, b32 debugBeep )
{
    int width = buffer->width;
    int height = buffer->height;
    
    int pitch = width * buffer->bytesPerPixel;
    u8 *row = (u8 *)buffer->memory;
    for( int y = 0; y < height; ++y )
    {
        u32 *pixel = (u32 *)row;
        for( int x = 0; x < width; ++x )
        {
            u8 b = (u8)(x + xOffset);
            u8 g = (u8)(y + yOffset);
            *pixel++ = debugBeep ? 0xFFFFFFFF : ((g << 16) | b);
        }
        row += pitch;
    }
}

internal void
DEBUGRenderPlayer( GameOffscreenBuffer *buffer, int playerX, int playerY )
{
    int top = playerY;
    int bottom = playerY + 10;
    u32 pitch = buffer->width * buffer->bytesPerPixel;

    for( int x = playerX; x < playerX+10; ++x )
    {
        u8 *pixel = ((u8 *)buffer->memory
                     + x*buffer->bytesPerPixel
                     + top*pitch);
        for( int y = top; y < bottom; ++y )
        {
            *(u32 *)pixel = 0xFFFFFFFF;
            pixel += pitch;
        }
    }
}

internal void
DEBUGOutputSineWave( GameState *gameState, GameAudioBuffer *buffer, int toneHz, b32 debugBeep )
{
    u32 toneAmp = 6000;
    u32 wavePeriod = buffer->samplesPerSecond / toneHz;

    s16 *sampleOut = buffer->samples;
    for( u32 sampleIndex = 0; sampleIndex < buffer->frameCount; ++sampleIndex )
    {
#if 1
        r32 sineValue = sinf( gameState->tSine );
        s16 sampleValue = (s16)(sineValue * toneAmp);
#else
        s16 sampleValue = 0;
#endif
        *sampleOut++ = debugBeep ? 32767 : sampleValue;
        *sampleOut++ = debugBeep ? 32767 : sampleValue;

        gameState->tSine += 2.f * PI32 * 1.0f / wavePeriod;
    }
}
#endif

