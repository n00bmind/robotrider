#include "robotrider.h"


internal void
RenderWeirdGradient( GameOffscreenBuffer *buffer, int xOffset, int yOffset )
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
            *pixel++ = ((g << 8) | b);
        }
        row += pitch;
    }
}

internal void
GameOutputSound( GameSoundBuffer *buffer, int toneHz )
{
    local_persistent r32 tSine;
    u32 toneAmp = 3000;
    u32 wavePeriod = buffer->samplesPerSecond / toneHz;

    s16 *sampleOut = buffer->samples;
    for( DWORD sampleIndex = 0; sampleIndex < buffer->sampleCount; ++sampleIndex )
    {
        r32 sineValue = sinf( tSine );
        s16 sampleValue = (s16)(sineValue * toneAmp);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;

        tSine += 2.f * PI32 * 1.0f / wavePeriod;
    }
}

internal void
GameUpdateAndRender( GameMemory *memory, GameInput *input, GameOffscreenBuffer *videoBuffer, GameSoundBuffer *soundBuffer )
{
    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );

    // Init storage for the game state
    GameState *gameState = (GameState *)memory->permanentStorage;
    if( !memory->isInitialized )
    {
        char *filename = __FILE__;
        DEBUGReadFileResult file = DEBUGPlatformReadEntireFile( filename );
        if( file.contents )
        {
            DEBUGPlatformWriteEntireFile( "test.out", file.contentSize, file.contents );
            DEBUGPlatformFreeFileMemory( file.contents );
        }
        
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        gameState->toneHz = 256;

        memory->isInitialized = true;
    }

    GameControllerInput *input0 = GetController( input, 0 );
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

    if( input0->aButton.endedDown )
    {
        gameState->greenOffset += 5;
    }

    GameOutputSound( soundBuffer, gameState->toneHz );
    RenderWeirdGradient( videoBuffer, gameState->blueOffset, gameState->greenOffset );
}
