#ifndef __ROBOTRIDER_H__
#define __ROBOTRIDER_H__ 

#if DEBUG
#define ASSERT(expression) if( !(expression) ) { *(int *)0 = 0; }
#else
#define ASSERT(expression)
#endif

#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES(value)*1024)

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

struct GameSoundBuffer
{
    int samplesPerSecond;
    int sampleCount;
    s16 *samples;
};

struct GameButtonState
{
    int halfTransitionCount;
    b32 endedDown;
};

struct GameControllerInput
{
    b32 isAnalog;
    
    r32 startX;
    r32 startY;
    r32 minX;
    r32 minY;
    r32 maxX;
    r32 maxY;
    r32 endX;
    r32 endY;

    union
    {
        GameButtonState buttons[8];
        struct
        {
            GameButtonState aButton;
            GameButtonState bButton;
            GameButtonState xButton;
            GameButtonState yButton;
            GameButtonState leftShoulder;
            GameButtonState rightShoulder;
            GameButtonState start;
            GameButtonState back;
        };
    };
};

struct GameInput
{
    r32 secondsElapsed;
    GameControllerInput controllers[4];
};

struct GameMemory
{
    b32 isInitialized;

    u64 permanentStorageSize;
    void *permanentStorage;     // NOTE Required to be cleared to zero at startup

    u64 transientStorageSize;
    void *transientStorage;     // NOTE Required to be cleared to zero at startup
};

internal void GameUpdateAndRender( GameInput *input, GameOffscreenBuffer *videoBuffer, GameSoundBuffer *soundBuffer );



//
// Other stuff
//

struct GameState
{
    int blueOffset;
    int greenOffset;
    int toneHz;
};



#endif /* __ROBOTRIDER_H__ */
