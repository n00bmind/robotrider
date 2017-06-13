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
GameUpdateAndRender( GameOffscreenBuffer *buffer )
{
    int blueOffset = 0;
    int greenOffset = 0;
    RenderWeirdGradient( buffer, blueOffset, greenOffset );
}
