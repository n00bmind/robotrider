#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 


//
// Services that the platform layer provides to the game
//




//
// Game entry points & data types for the platform layer
//
struct GameOffscreenBuffer
{
    void *memory;
    int width;
    int height;
    int bytesPerPixel;
};


internal void GameUpdateAndRender( GameOffscreenBuffer *buffer );




#endif /* __ROBOTRIDER_H__ */
