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
            u8 b = (x + xOffset);
            u8 g = (y + yOffset);
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
    // Init storage for the game state
    GameState *gameState = (GameState *)memory->permanentStorage;
    if( !memory->isInitialized )
    {
        gameState->blueOffset = 0;
        gameState->greenOffset = 0;
        gameState->toneHz = 256;
    }

    GameControllerInput *input0 = &input->controllers[0];
    if( input0->isAnalog )
    {
        toneHz = 256 + (int)(128.f * input0->endY);
        blueOffset += (int)(4.f * input0->endX);
    }
    else
    {

    }

    if( input0->aButton.endedDown )
    {
        greenOffset += 5;
    }

    GameOutputSound( soundBuffer, toneHz );
    RenderWeirdGradient( videoBuffer, blueOffset, greenOffset );
}
