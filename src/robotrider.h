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


inline u32 SafeTruncU64( u64 value )
{
    ASSERT( value <= 0xFFFFFFFF );
    u32 result = (u32)value;
    return result;
}

//
// Services that the platform layer provides to the game
//
#if DEBUG
struct DEBUGReadFileResult
{
    u32 contentSize;
    void *contents;
};
internal DEBUGReadFileResult DEBUGPlatformReadEntireFile( char *filename );
internal void DEBUGPlatformFreeFileMemory( void *bitmapMemory );
internal b32 DEBUGPlatformWriteEntireFile( char*filename, u32 memorySize, void *memory );
#endif


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
    u32 samplesPerSecond;
    u32 sampleCount;
    s16 *samples;
};

struct GameStickState
{
    // -1.0 to 1.0 range (negative is left/down, positive is up/right)
    r32 startX;
    r32 startY;
    r32 avgX;
    r32 avgY;
    r32 endX;
    r32 endY;
};

struct GameButtonState
{
    int halfTransitionCount;
    b32 endedDown;
};

struct GameControllerInput
{
    b32 isConnected;
    b32 isAnalog;
    
    GameStickState leftStick;
    GameStickState rightStick;

    union
    {
        struct
        {
            GameButtonState dUp;
            GameButtonState dDown;
            GameButtonState dLeft;
            GameButtonState dRight;

            GameButtonState aButton;
            GameButtonState bButton;
            GameButtonState xButton;
            GameButtonState yButton;

            GameButtonState leftThumb;
            GameButtonState rightThumb;
            GameButtonState leftShoulder;
            GameButtonState rightShoulder;

            GameButtonState start;
            GameButtonState back;
        };
        GameButtonState buttons[14];
    };
};

struct GameInput
{
    r32 secondsElapsed;
    GameControllerInput _controllers[5];
};

inline GameControllerInput *
GetController( GameInput *input, int controllerIndex )
{
    ASSERT( controllerIndex < ARRAYCOUNT( input->_controllers ) );
    GameControllerInput *result = &input->_controllers[controllerIndex];
    return result;
}

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
