#include "robotrider.h"


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

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );

    // Init storage for the game state
    GameState *gameState = (GameState *)memory->permanentStorage;
    if( !memory->isInitialized )
    {
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        gameState->toneHz = 256;
        gameState->tSine = 0.0f;

        gameState->playerX = 100;
        gameState->playerY = 100;

        memory->isInitialized = true;
    }

    GameControllerInput *input0 = GetController( input, 0 );
#if 0
    if( input0->isAnalog )
    {
        gameState->blueOffset += (int)(4.f * input0->leftStick.avgX);
        gameState->toneHz = 256 + (int)(128.f * input0->leftStick.avgY);
    }
    else
    {
        if( input0->dLeft.endedDown )
        {
            gameState->blueOffset -= 1;
        }
        if( input0->dRight.endedDown )
        {
            gameState->blueOffset += 1;
        }
    }

    gameState->playerX += (int)(4.0f * input0->leftStick.avgX);
    gameState->playerY -= (int)(4.0f * input0->leftStick.avgY);

    if( input0->aButton.endedDown )
    {
        gameState->playerY += 10;
    }

    DEBUGOutputSineWave( gameState, audioBuffer, gameState->toneHz, debugBeep );
    DEBUGRenderWeirdGradient( videoBuffer, gameState->blueOffset, gameState->greenOffset, debugBeep );
    DEBUGRenderPlayer( videoBuffer, gameState->playerX, gameState->playerY );

    if( input->mouseButtons[0].endedDown )
    {
        DEBUGRenderPlayer( videoBuffer, input->mouseX, input->mouseY );
    }
#endif

    renderCommands->width = 800;
    renderCommands->height = 600;
}
