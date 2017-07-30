#include "robotrider.h"
#include "renderer.cpp"


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

PlatformAPI platform;

internal void
InitializeArena( MemoryArena *arena, u8 *base, mem_idx size )
{
    arena->base = base;
    arena->size = size;
    arena->used = 0;
}

#define PUSH_STRUCT(arena, type) (type *)_PushSize( arena, sizeof(type) )
#define PUSH_ARRAY(arena, count, type) (type *)_PushSize( arena, (count)*sizeof(type) )

internal void *
_PushSize( MemoryArena *arena, mem_idx size )
{
    ASSERT( arena->used + size <= arena->size );
    // TODO Clear to zero option

    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    platform = memory->platformAPI;

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );

    // Init storage for the game state
    GameState *gameState = (GameState *)memory->permanentStorage;
    if( !memory->isInitialized )
    {
        InitializeArena( &gameState->worldArena,
                         (u8 *)memory->permanentStorage + sizeof(GameState),
                         memory->permanentStorageSize - sizeof(GameState) );

        gameState->world = PUSH_STRUCT( &gameState->worldArena, World );
        World *worldInit = gameState->world;

        worldInit->dude = PUSH_STRUCT( &gameState->worldArena, FlyingDude );
        FlyingDude &dude = *worldInit->dude;
        dude =
        {
            {},
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
        dude.renderGroup = CreateRenderGroup( dude );

        worldInit->cubeCount = 16*16;
        worldInit->cubes = PUSH_ARRAY( &gameState->worldArena, worldInit->cubeCount, CubeThing );

        for( u32 i = 0; i < worldInit->cubeCount; ++i )
        {
            r32 transX = (((i32)i % 16) - 8) * 2.0f;
            r32 transY = ((i32)i / 16) * 2.0f;

            CubeThing &cube = worldInit->cubes[i];
            cube =
            {
                {},
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
            cube.mTransform = Translation( { transX, transY, -1.0f } );
            cube.renderGroup = CreateRenderGroup( cube );
        }

        memory->isInitialized = true;
    }

    float dt = input->secondsElapsed;
    World *world = gameState->world;
    GameControllerInput *input0 = GetController( input, 0 );

    FlyingDude &dude = *world->dude;
    v3 pPlayer = GetTranslation( dude.mTransform );
    m4 mPlayerRot = GetRotation( dude.mTransform );

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

    if( input0->rightStick.avgX )
    {
        dude.pitch += input0->rightStick.avgY / 15.f * dt;
        dude.yaw += -input0->rightStick.avgX / 15.f * dt; 
        mPlayerRot = ZRotation( dude.yaw ) * XRotation( dude.pitch );
    }

    pPlayer = pPlayer + mPlayerRot * vPlayerDelta;
    dude.mTransform = RotPos( mPlayerRot, pPlayer );
    PushRenderGroup( renderCommands, &dude.renderGroup );

    // Create a chasing camera
    // TODO Use a PID controller
    v3 pCam = dude.mTransform * V3( 0, -2, 1 );
    v3 pLookAt = dude.mTransform * V3( 0, 1, 0 );
    v3 vUp = GetColumn( dude.mTransform, 2 ).xyz; //{ 0, 0, 1 }; 
    renderCommands.mCamera = CameraLookAt( pCam, pLookAt, vUp );


    for( u32 i = 0; i < world->cubeCount; ++i )
    {
        CubeThing &cube = world->cubes[i];
        PushRenderGroup( renderCommands, &cube.renderGroup );
    }
}
