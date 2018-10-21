/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include "game.h"

#include "data_types.h"
#include "meshgen.h"
#include "world.h"
#include "asset_loaders.h"
#include "wfc.h"
#include "editor.h"
#include "robotrider.h"

#include "renderer.cpp"

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui_demo.cpp"     // TODO Remove!

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x) ASSERT(x)
void* LibMalloc( sz size );
void* LibRealloc( void* p, sz newSize );
void  LibFree( void* p );
#define STBI_MALLOC(sz)           LibMalloc( sz )
#define STBI_REALLOC(p,newsz)     LibRealloc( p, newsz )
#define STBI_FREE(p)              LibFree( p )
#define STBI_ONLY_BMP
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#include "stb/stb_image.h"

#include "ui.cpp"
#include "console.cpp"
#include "asset_loaders.cpp"
#include "wfc.cpp"
#include "meshgen.cpp"
#include "world.cpp"
#include "editor.cpp"



PlatformAPI globalPlatform;
internal GameConsole *gameConsole;
// TODO Remove
internal MemoryArena* auxArena;


// FIXME Put these in some kind of more general pool (similar to the MeshPool)
void*
LibMalloc( sz size )
{
    void* result = PUSH_SIZE( auxArena, size );
    return result;
}

void*
LibRealloc( void* p, sz newSize )
{
    // FIXME Just leak for now
    void* result = LibMalloc( newSize );
    return result;
}

void
LibFree( void* p )
{
    return;
}


LIB_EXPORT
GAME_LOG_CALLBACK(GameLogCallback)
{
    if( gameConsole )
        ConsoleLog( gameConsole, msg );
}

// TODO Remove
LIB_EXPORT
GAME_SETUP_AFTER_RELOAD(GameSetupAfterReload)
{
    //ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    //GameState *gameState = (GameState *)memory->permanentStorage;
    //gameConsole = &gameState->gameConsole;

    //// Re-set platform's ImGui context
    //ImGui::SetCurrentContext( gameState->imGuiContext );

    // Do this in a real test suite
    TestDataTypes();
}

LIB_EXPORT
GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    globalPlatform = *memory->platformAPI;
#if !RELEASE
    DebugState* debugState = (DebugState*)memory->debugStorage;
#endif

    TIMED_BLOCK;

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState *gameState = (GameState *)memory->permanentStorage;

    // Init game arenas & world state
    if( !memory->isInitialized )
    {
        InitializeArena( &gameState->worldArena,
                         (u8 *)memory->permanentStorage + sizeof(GameState),
                         memory->permanentStorageSize - sizeof(GameState) );

        InitializeArena( &gameState->transientArena,
                         (u8 *)memory->transientStorage,
                         memory->transientStorageSize );

        auxArena = &gameState->worldArena;

        gameState->world = PUSH_STRUCT( &gameState->worldArena, World );
        InitWorld( gameState->world, &gameState->worldArena, &gameState->transientArena );

        memory->isInitialized = true;
    }

    TemporaryMemory tempMemory = BeginTemporaryMemory( &gameState->transientArena );

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    if( input->executableReloaded )
    {
        // TODO Check if these are all ok here so that we can remove GAME_SETUP_AFTER_RELOAD
        gameConsole = &gameState->gameConsole;
        // Re-set platform's ImGui context
        ImGui::SetCurrentContext( memory->imGuiContext );

        // FIXME
        auxArena = &gameState->worldArena;

#if !RELEASE
        memory->DEBUGglobalEditing = true;
        InitEditor( { width, height }, &gameState->DEBUGeditorState, gameState->world,
                    &gameState->worldArena, &gameState->transientArena );
#endif
    }

    {
        GameInput* gameInput = input;

#if !RELEASE
        GameInput dummyInput;
        if( memory->DEBUGglobalEditing )
        {
            dummyInput =
            {
                input->executableReloaded,
                input->frameElapsedSeconds,
                input->totalElapsedSeconds,
                input->frameCounter,
            };
            gameInput = &dummyInput;
        }
#endif

        UpdateAndRenderWorld( gameInput, memory, renderCommands );
    }

#if !RELEASE
    float fps = ImGui::GetIO().Framerate; //1.f / input->frameElapsedSeconds;
    float frameTime = 1000.f / fps;
    char statsText[1024];
    snprintf( statsText, ARRAYCOUNT(statsText),
              "Frame ms.: %.3f (%.1f FPS)   Live entitites %u   Primitives %u   Vertices %u (+ %u)  DrawCalls %u",
              frameTime, fps, debugState->totalEntities,
              debugState->totalPrimitiveCount, debugState->totalVertexCount, debugState->totalGeneratedVerticesCount,
              debugState->totalDrawCalls );

    if( memory->DEBUGglobalEditing )
    {
        UpdateAndRenderEditor( input, memory, renderCommands, statsText, &gameState->worldArena, &gameState->transientArena );
    }
    else if( memory->DEBUGglobalDebugging )
    {
        DrawConsole( &gameState->gameConsole, width, height, statsText );
        DrawPerformanceCounters( memory, width, height );
    }
    else
        DrawStats( width, height, statsText );
#endif

    EndTemporaryMemory( tempMemory );

    CheckArena( &gameState->worldArena );
    CheckArena( &gameState->transientArena );
}


#if !RELEASE
//const u32 DEBUGglobalCountersCount = __COUNTER__;
DebugCycleCounter DEBUGglobalCounters[__COUNTER__];

LIB_EXPORT
DEBUG_GAME_FRAME_END(DebugGameFrameEnd)
{
    ASSERT( ARRAYCOUNT(DEBUGglobalCounters) < ARRAYCOUNT(DebugState::counterLogs) );

    DebugState* debugState = (DebugState*)memory->debugStorage;
    debugState->counterLogsCount = 0;

    // TODO Counter stats
    u32 snapshotIndex = 0; //debugState->snapshotIndex;
    for( u32 i = 0; i < ARRAYCOUNT(DEBUGglobalCounters); ++i )
    {
        DebugCycleCounter& source = DEBUGglobalCounters[i];
        DebugCounterLog& dest = debugState->counterLogs[debugState->counterLogsCount++];

        dest.filename = source.filename;
        dest.function = source.function;
        dest.lineNumber = source.lineNumber;
        UnpackAndResetFrameCounter( source, &dest.snapshots[snapshotIndex].cycleCount, &dest.snapshots[snapshotIndex].hitCount );
    }

    debugState->snapshotIndex++;
    if( debugState->snapshotIndex >= ARRAYCOUNT(DebugCounterLog::snapshots) )
        debugState->snapshotIndex = 0;
}
#endif
