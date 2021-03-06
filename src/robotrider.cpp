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

#include "meshgen.h"
#include "world.h"
#include "wfc.h"
#include "asset_loaders.h"
#include "editor.h"
#include "robotrider.h"
#include "util.h"

#include "renderer.cpp"

#pragma warning( push )
#pragma warning( disable: 4365 )
#pragma warning( disable: 4774 )

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_widgets.cpp"
#include "imgui/imgui_demo.cpp"

#if !NON_UNITY_BUILD
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#endif
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
#include "stb/stb_image.h"

#pragma warning( pop )

#include "ui.h"
#include "util.cpp"
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
#if !RELEASE
bool* DEBUGglobalQuit;
#endif


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
    DEBUGglobalQuit = &memory->platformAPI->DEBUGquit;
#endif

    TIMED_FUNC;

    ASSERT( sizeof(GameState) <= memory->permanentStorageSize );
    GameState* gameState = (GameState*)memory->permanentStorage;
    ASSERT( sizeof(TransientState) <= memory->transientStorageSize );
    TransientState* transientState = (TransientState*)memory->transientStorage;

    // Init game arenas & world state
    if( !memory->isInitialized )
    {
        InitArena( &gameState->worldArena,
                   (u8 *)memory->permanentStorage + sizeof(GameState),
                   memory->permanentStorageSize - sizeof(GameState) );

        InitArena( &gameState->transientArena,
                   (u8 *)memory->transientStorage + sizeof(TransientState),
                   memory->transientStorageSize - sizeof(TransientState) );

        // TODO Remove this
        auxArena = &gameState->worldArena;

        gameState->world = PUSH_STRUCT( &gameState->worldArena, World );
        InitWorld( gameState->world, &gameState->worldArena, &gameState->transientArena );

        memory->isInitialized = true;
    }

    u16 width = renderCommands->width;
    u16 height = renderCommands->height;

    if( input->gameCodeReloaded )
    {
        // TODO Check if these are all ok here so that we can remove GAME_SETUP_AFTER_RELOAD
        gameConsole = &gameState->gameConsole;
        // Set platform's ImGui context (and allocators!)
        ImGui::SetCurrentContext( memory->imGuiContext );
        ImGui::SetAllocatorFunctions( memory->imGuiAllocFunc, memory->imGuiFreeFunc );

        // FIXME
        auxArena = &gameState->worldArena;

#if 0 //!RELEASE
        memory->DEBUGglobalEditing = false;
        InitEditor( { width, height }, gameState, &gameState->DEBUGeditorState, transientState,
                    &gameState->worldArena, &gameState->transientArena );
#else
        memory->DEBUGglobalEditing = true;
        gameState->DEBUGeditorState.selectedTest.index = 1;
#endif
    }

    TemporaryMemory frameMemory = BeginTemporaryMemory( &gameState->transientArena );

    {
        GameInput* gameInput = input;

#if !RELEASE
        GameInput dummyInput;
        if( memory->DEBUGglobalEditing )
        {
            dummyInput =
            {
                input->gameCodeReloaded,
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
    DebugState* debugState = (DebugState*)memory->debugStorage;

    float fps = ImGui::GetIO().Framerate; //1.f / input->frameElapsedSeconds;
    float frameTime = 1000.f / fps;
    char statsText[1024];
    snprintf( statsText, ARRAYCOUNT(statsText),
              "Frame ms.: %.3f (%.1f FPS)   Live entitites %u   Meshes %u   Instances %u   Primitives %u   Vertices %u (+ %u)  DrawCalls %u",
              frameTime, fps, debugState->totalEntities, debugState->totalMeshCount, debugState->totalInstanceCount,
              debugState->totalPrimitiveCount, debugState->totalVertexCount, debugState->totalGeneratedVerticesCount,
              debugState->totalDrawCalls );

    if( memory->DEBUGglobalEditing )
    {
        UpdateAndRenderEditor( *input, gameState, transientState, debugState, renderCommands, statsText,
                               &gameState->worldArena, &gameState->transientArena );
    }
     
    if( memory->DEBUGglobalDebugging )
    {
        bool focusInputBox = !memory->DEBUGglobalWasDebugging;
        DrawConsole( &gameState->gameConsole, width, height, focusInputBox );
        DrawPerformanceCountersWindow( debugState, width, height, &gameState->transientArena );
    }
    memory->DEBUGglobalWasDebugging = memory->DEBUGglobalDebugging;
    
    if( !memory->DEBUGglobalEditing )
        DrawStats( width, height, statsText );
#endif

    EndTemporaryMemory( frameMemory );

    CheckTemporaryBlocks( &gameState->worldArena );
    CheckTemporaryBlocks( &gameState->transientArena );
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

    // TODO Monitor and report big changes in this ratio
    f64 cyclesPerMillisecond = (f64)frameInfo.totalFrameCycles / frameInfo.totalFrameSeconds / 1000.0 ;

    persistent f64 minCPM = F64MAX;
    persistent f64 maxCPM = F64MIN;

    minCPM = Min( minCPM, cyclesPerMillisecond );
    maxCPM = Max( maxCPM, cyclesPerMillisecond );
    debugState->avgCyclesPerMillisecond = (minCPM + maxCPM) * 0.5;

    // TODO Counter stats
    int snapshotIndex = debugState->counterSnapshotIndex;
    for( int i = 0; i < ARRAYCOUNT(DEBUGglobalCounters); ++i )
    {
        DebugCycleCounter* counter = DEBUGglobalCounters + i;
        DebugCounterLog* log = debugState->counterLogs + debugState->counterLogsCount++;

        LogFrameCounter( counter, log, snapshotIndex, cyclesPerMillisecond );
    }

    debugState->counterSnapshotIndex++;
    if( debugState->counterSnapshotIndex >= ARRAYCOUNT(DebugCounterLog::snapshots) )
        debugState->counterSnapshotIndex = 0;
}
#endif
