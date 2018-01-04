#include "robotrider.h"
#include "renderer.cpp"
#include "ui.cpp"
#include "console.cpp"

PlatformAPI globalPlatform;


internal void
DEBUGDrawStats( u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImVec2 statsPos( 0, 0 );
    ImVec4 statsTextColor( .0f, .0f, .0f, 1.0f );
    ImVec4 statsBgColor( 0.5f, 0.5f, 0.5f, 0.05f );

    ImGui::SetNextWindowPos( statsPos, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, statsBgColor );
    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( statsTextColor, statsText );
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

LIB_EXPORT
GAME_SETUP_AFTER_RELOAD(GameSetupAfterReload)
{
    // Re-set platform's ImGui context
    ImGui::SetCurrentContext( gameState->imGuiContext );
}

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    globalPlatform = memory->platformAPI;

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState *gameState = (GameState *)memory->permanentStorage;

    // Init game arena
    if( !memory->isInitialized )
    {
        InitializeArena( &gameState->gameArena,
                         (u8 *)memory->permanentStorage + sizeof(GameState),
                         memory->permanentStorageSize - sizeof(GameState) );

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

        tranState->dude = PUSH_STRUCT( &gameState->gameArena, FlyingDude );
        FlyingDude &dude = *tranState->dude;
        dude =
        {
            {
                {  0.5f,   -0.5f,  0.0f, },
                { -0.5f,   -0.5f,  0.0f, },
                {  0.0f,    1.0f,  0.0f, },
                //{  0.0f,     0.5f	 0.5f, },
            },
            {
                0, 1, 2,
                //2, 1, 3,
                //2, 3, 0,
                //3, 1, 0,
            },
        };
        dude.mTransform = Identity();

        int rowSize = 31;
        int rowHalf = rowSize >> 1;
        tranState->cubeCount = rowSize * rowSize;
        tranState->cubes = PUSH_ARRAY( &tranState->transientArena, tranState->cubeCount, CubeThing );

        for( u32 i = 0; i < tranState->cubeCount; ++i )
        {
            CubeThing &cube = tranState->cubes[i];
            cube =
            {
                {
                    { -0.5f,    -0.5f,      0.0f },
                    { -0.5f,     0.5f,      0.0f },
                    {  0.5f,    -0.5f,      0.0f },
                    {  0.5f,     0.5f,      0.0f },
                },
                {
                    0, 1, 2,
                    2, 1, 3
                },
            };

            r32 transX = (((i32)i % rowSize) - rowHalf) * 2.0f;
            r32 transY = ((i32)i / rowSize) * 2.0f;

            cube.mTransform = Translation( { transX, transY, -1.0f } );
        }

        tranState->isInitialized = true;
    }

    TemporaryMemory renderMemory = BeginTemporaryMemory( &tranState->transientArena );

#if DEBUG
    u16 width = renderCommands.width;
    u16 height = renderCommands.height;

    float fps = 1.f / input->frameElapsedSeconds;
    char statsText[1024];
    snprintf( statsText, 1024, "FPS %.1f", fps );   // Seems to be crossplatform, so it's good enough for now

    if( gameState->DEBUGglobalDebugging )
    {
        DrawConsole( &gameState->gameConsole, width, height, statsText );
        ImGui::SetNextWindowPos( ImVec2( width - 500.f, height - 300.f ) );
        ImGui::ShowUserGuide();
    }
    else
        DEBUGDrawStats( width, height, statsText );
#endif

    float dt = input->frameElapsedSeconds;
    GameControllerInput *input0 = GetController( input, 0 );

    PushClear( renderCommands, { 0.95f, 0.95f, 0.95f, 1.0f } );

    FlyingDude &dude = *tranState->dude;
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
        gameState->playerPitch += input0->rightStick.avgY / 15.f * dt;
        gameState->playerYaw += -input0->rightStick.avgX / 15.f * dt; 
    }

    m4 mPlayerRot = ZRotation( gameState->playerYaw ) * XRotation( gameState->playerPitch );
    pPlayer = pPlayer + mPlayerRot * vPlayerDelta;
    dude.mTransform = RotPos( mPlayerRot, pPlayer );
    PushRenderGroup( renderCommands, dude );

    gameState->pPlayer = pPlayer;

    // Create a chasing camera
    // TODO Use a PID controller
    v3 pCam = dude.mTransform * V3( 0, -2, 1 );
    v3 pLookAt = dude.mTransform * V3( 0, 1, 0 );
    v3 vUp = GetColumn( dude.mTransform, 2 ).xyz;
    renderCommands.mCamera = CameraLookAt( pCam, pLookAt, vUp );

    for( u32 i = 0; i < tranState->cubeCount; ++i )
    {
        CubeThing *cube = tranState->cubes + i;
        PushRenderGroup( renderCommands, *cube );
    }

    EndTemporaryMemory( renderMemory );

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

