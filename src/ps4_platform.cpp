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

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_widgets.cpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kernel.h>

size_t sceLibcHeapSize = 16 * 1024 * 1024;
unsigned int sceLibcHeapDebugFlags = SCE_LIBC_HEAP_DEBUG_SHORTAGE;

#include <pad.h>
#include <gnmx.h>
#include <gnmx/shader_parser.h>
#include <video_out.h>
#include <gpu_debugger.h>

#include "ps4_platform.h"

using namespace sce;


PlatformAPI globalPlatform;
internal PS4State globalPlatformState;


#include "ps4_renderer.h"
#include "ps4_renderer.cpp"


internal const r32 targetFramerateHz = 60;


internal void*
PS4AllocAndMap( void* address, sz size, int memoryType, sz alignment /* = 0 */, 
                int protection /* = SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW */ )
{
    // FIXME Switch to getting this info from sceKernelVirtualQuery
    ASSERT( globalPlatformState.nextFreeMemoryBlock < ARRAYCOUNT(globalPlatformState.memoryBlocks) );
    PS4MemoryBlock *result = globalPlatformState.memoryBlocks + globalPlatformState.nextFreeMemoryBlock++;
    result->address = address;

    ASSERT( size > 0 );
    const u32 pageSize = 16*1024;

    if( alignment > pageSize )
        result->size = Align( size, alignment );
    else
        result->size = Align( size, pageSize );

    int ret = sceKernelAllocateMainDirectMemory( result->size, alignment, memoryType, &result->physicalOffset );
    ASSERT( ret == 0 );

    // NOTE Never specify 0 for the 'protection' field! Non-consistent page faults will ensue!
    ret = sceKernelMapDirectMemory( &result->address, result->size, protection,
                                    (address ? SCE_KERNEL_MAP_FIXED : 0) | SCE_KERNEL_MAP_NO_OVERWRITE,
                                    result->physicalOffset, alignment );
    ASSERT( ret == 0 );

    return result->address;
}

internal void
PS4Free( void* address )
{
    PS4MemoryBlock* block = nullptr;
    for( u32 i = 0; i < ARRAYCOUNT(globalPlatformState.memoryBlocks); ++i )
    {
        block = &globalPlatformState.memoryBlocks[i];
        if( block->address == address )
            break;
    }
    ASSERT( block );

    int ret = sceKernelMunmap( block->address, block->size );
    ASSERT( ret == 0 );
    block->address = nullptr;

    ret = sceKernelReleaseDirectMemory( block->physicalOffset, block->size );
    ASSERT( ret == 0 );
    block->physicalOffset = 0;

    block->size = 0;
}


internal sz
PS4JoinPaths( const char* pathBase, const char* relativePath, char* destination, bool addTrailingSlash = false )
{
    const char* format = "%s/%s";
    if( relativePath[0] == '/' )
        format = "%s%s";

    snprintf( destination, MAX_PATH, format, pathBase, relativePath );
    sz len = strlen( destination );
    ASSERT( len < MAX_PATH );
    
    if( addTrailingSlash )
    {
        if( destination[len-1] != '/' )
        {
            if( len < MAX_PATH )
                destination[len++] = '/';
            if( len < MAX_PATH )
                destination[len++] = 0;
        }
    }

    return len;
}

internal void
PS4ResolvePaths( PS4State* platformState )
{
#if RELEASE
    NOT_IMPLEMENTED
#else
    snprintf( platformState->binariesPath, MAX_PATH, "/app0/bin/ORBIS" );
    snprintf( platformState->assetDataPath, MAX_PATH, "/app0/data" );
#endif
}

internal void
PS4BuildAbsolutePath( const char* filename, PS4Path pathType, char *outPath )
{
    switch( pathType )
    {
        case PS4Path::Binary:
            PS4JoinPaths( globalPlatformState.binariesPath, filename, outPath );
            break;
        case PS4Path::Asset:
            PS4JoinPaths( globalPlatformState.assetDataPath, filename, outPath );
            break;
        INVALID_DEFAULT_CASE
    }
}

PLATFORM_LOG(PS4Log)
{
    char buffer[1024];

    va_list args;
    va_start( args, fmt );
    vsnprintf( buffer, ARRAYCOUNT( buffer ), fmt, args );
    va_end( args );

    printf( "%s\n", buffer );

    if( globalPlatformState.gameCode.LogCallback )
        globalPlatformState.gameCode.LogCallback( buffer );
}


DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPS4FreeFileMemory)
{
    if( memory )
    {
        PS4Free( memory );
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPS4ReadEntireFile)
{
    DEBUGReadFileResult result = {};

    char absolutePath[MAX_PATH];
    if( filename[0] != '/' )
    {
        // If path is relative, use assets location to complete it
        PS4BuildAbsolutePath( filename, PS4Path::Asset, absolutePath );
        filename = absolutePath;
    }

    int fileHandle = sceKernelOpen( filename, SCE_KERNEL_O_RDONLY, 0 );
    if( fileHandle > 0 )
    {
        int ret = 0;
        SceKernelStat st;
        ret = sceKernelFstat( fileHandle, &st );

        if( ret == 0 )
        {
            sz fileSize = st.st_size;
            result.contents = PS4AllocAndMap( 0, fileSize + 1, SCE_KERNEL_WB_ONION, 0, SCE_KERNEL_PROT_CPU_RW );

            if( result.contents )
            {
                sz bytesRead = sceKernelRead( fileHandle, result.contents, fileSize );
                if( bytesRead > 0 )
                {
                    // Null-terminate to help when handling text files
                    *((u8 *)result.contents + fileSize) = '\0';
                    result.contentSize = fileSize + 1;
                }
                else
                {
                    LOG( "ERROR: ReadEntireFile failed" );
                    DEBUGPS4FreeFileMemory( result.contents );
                    result.contents = nullptr;
                }
            }
            else
            {
                LOG( "ERROR: Couldn't allocate buffer for file contents" );
            }
        }
        else
        {
            LOG( "ERROR: Failed querying file size (%#x)", ret );
        }
    }
    else
    {
        LOG( "ERROR: Failed opening file '%s' for reading (%#x)", filename, fileHandle );
    }

    return result;
}


internal
PLATFORM_ADD_NEW_JOB(PS4AddNewJob)
{
    // NOTE Single producer
    ASSERT( queue->nextJobToWrite != queue->nextJobToRead ||
            queue->jobs[queue->nextJobToRead].callback == nullptr );

    PlatformJobQueueJob& job = queue->jobs[queue->nextJobToWrite];
    job = { callback, userData };
    ++queue->completionTarget;

    MEMORY_WRITE_BARRIER;

    queue->nextJobToWrite = (queue->nextJobToWrite + 1) % ARRAYCOUNT(queue->jobs);
    // TODO
    // Check https://www.ibm.com/developerworks/systems/library/es-win32linux-sem.html
    //ReleaseSemaphore( queue->semaphore, 1, 0 );
}

internal
PLATFORM_COMPLETE_ALL_JOBS(PS4CompleteAllJobs)
{
    //
    // FIXME Assert that this is only called from the main thread!
    //
    //
    //while( queue->completionCount < queue->completionTarget )
    {
        // Main thread 'worker' index is always 0
        //PS4DoNextQueuedJob( queue, 0 );
    }

    queue->completionTarget = 0;
    queue->completionCount = 0;
}

// TODO 
internal void
PS4InitJobQueue( PlatformJobQueue* queue,
                 PS4WorkerThreadContext* threadContexts, u32 threadCount )
{
    *queue = {0};
    //queue->semaphore = CreateSemaphoreEx( 0, 0, threadCount,
                                          //0, 0, SEMAPHORE_ALL_ACCESS );

    for( u32 i = 0; i < threadCount; ++i )
    {
        threadContexts[i] =
        {
            i + 1,      // Worker thread index 0 is reserved for the main thread!
            queue,
        };

        u32 threadId;
        //HANDLE handle = CreateThread( 0, MEGABYTES(1),
                                      //Win32WorkerThreadProc,
                                      //&threadContexts[i], 0, &threadId );
        //CloseHandle( handle );
    }
}

internal PS4GameCode
PS4LoadGameCode( const char* prxPath, GameMemory* gameMemory )
{
    PS4GameCode result = {};
    
//     SceKernelStat bla = {};
//     if( sceKernelStat( prxPath, &bla ) == 0 )
//         result.lastPrxWriteTime = bla.st_mtim;

    result.gameCodePrx = sceKernelLoadStartModule( prxPath, 0, nullptr, 0, nullptr, nullptr );
    if( result.gameCodePrx > 0 )
    {
        sceKernelDlsym( result.gameCodePrx, "GameSetupAfterReload", (void**)&result.SetupAfterReload );
        sceKernelDlsym( result.gameCodePrx, "GameUpdateAndRender", (void**)&result.UpdateAndRender );
        sceKernelDlsym( result.gameCodePrx, "GameLogCallback", (void**)&result.LogCallback );
        sceKernelDlsym( result.gameCodePrx, "DebugGameFrameEnd", (void**)&result.DebugFrameEnd );

        result.isValid = result.UpdateAndRender != nullptr;
    }

    if( result.SetupAfterReload )
        result.SetupAfterReload( gameMemory );

    if( !result.UpdateAndRender )
        result.UpdateAndRender = GameUpdateAndRenderStub;

    return result;
}

internal void
PS4InitInput( PS4State* state )
{
    int ret = sceUserServiceInitialize( nullptr );
    ASSERT( ret == 0 );

    // NOTE This model for user management is recommended to use 'InitialUserAlwaysLoggedIn'
    // when creating app packages
    SceUserServiceUserId userId;
    ret = sceUserServiceGetInitialUser( &userId );
    ASSERT( ret == 0 );

    ret = scePadInit();
    ASSERT( ret == 0 );

    i32 handle = scePadOpen( userId, SCE_PAD_PORT_TYPE_STANDARD, 0, nullptr );
    ASSERT( handle > 0 );

    for( u32 i = 0; i < ARRAYCOUNT(state->controllerInfos); ++i )
    {
        if( i == PLATFORM_KEYMOUSE_CONTROLLER_SLOT )
            continue;

        state->controllerInfos[i].handle = handle;
        ret = scePadGetControllerInformation( handle, &state->controllerInfos[i].pad );
        ASSERT( ret == 0 );

        // Ready Player 1
        break;
    }
}

internal void
PS4PrepareInputData( GameInput** oldInput, GameInput** newInput,
                     r32 frameElapsedSeconds , r32 totalElapsedSeconds, u32 deltaFrameCounter )
{
    GameInput* temp = *newInput;
    *newInput = *oldInput;
    *oldInput = temp;

    (*newInput)->mouseX = (*oldInput)->mouseX;
    (*newInput)->mouseY = (*oldInput)->mouseY;
    (*newInput)->mouseZ = (*oldInput)->mouseZ;
    for( u32 i = 0; i < ARRAYCOUNT((*newInput)->mouseButtons); ++i )
        (*newInput)->mouseButtons[i] = (*oldInput)->mouseButtons[i];

    (*newInput)->executableReloaded = ((*newInput)->frameCounter == 0);
    // Prevent huge skips in physics etc. while debugging
    if( frameElapsedSeconds >= 1.f )
    {
        (*newInput)->frameElapsedSeconds = 1.f / targetFramerateHz;
        (*newInput)->totalElapsedSeconds = (*oldInput)->totalElapsedSeconds + (*newInput)->frameElapsedSeconds;
    }
    else
    {
        (*newInput)->frameElapsedSeconds = frameElapsedSeconds;
        (*newInput)->totalElapsedSeconds = totalElapsedSeconds;
    }
    (*newInput)->frameCounter = (*oldInput)->frameCounter + deltaFrameCounter;
}

internal r32
PS4ProcessStickValue( u8 newValue, u8 deadZoneHalfWidth )
{
    // TODO Cube the values as explained in the XInput help
    // to give more sensitivity to the movement?

    r32 result = 0;
    int intValue = newValue - 0x80;

    // TODO Does hardware have a round deadzone?
    if( intValue < -deadZoneHalfWidth )
    {
        result = (r32)(intValue + deadZoneHalfWidth) / (128.0f - deadZoneHalfWidth);
    }
    else if( intValue > deadZoneHalfWidth )
    {
        result = (r32)(intValue - deadZoneHalfWidth) / (127.0f - deadZoneHalfWidth);
    }

    return result;
}

internal void
PS4ReadControllerStates( PS4State* state, GameInput* oldInput, GameInput* newInput )
{
    for( u32 i = 0; i < ARRAYCOUNT(state->controllerInfos); ++i )
    {
        if( i == PLATFORM_KEYMOUSE_CONTROLLER_SLOT )
            continue;

        PS4ControllerInfo& info = state->controllerInfos[i];
        if( info.handle > 0 )
        {
            ScePadData data;
            int ret = scePadReadState( info.handle, &data );
            ASSERT( ret == 0 );

            GameControllerInput* newC = GetController( newInput, i );
            GameControllerInput* oldC = GetController( oldInput, i );
            newC->isConnected = data.connected;

            if( newC->isConnected )
            {
                newC->leftStick.startX = oldC->leftStick.endX;
                newC->leftStick.endX = PS4ProcessStickValue( data.leftStick.x, info.pad.stickInfo.deadZoneLeft );
                newC->leftStick.avgX = (newC->leftStick.startX + newC->leftStick.endX) / 2;

                newC->leftStick.startY = oldC->leftStick.endY;
                newC->leftStick.endY = PS4ProcessStickValue( data.leftStick.y, info.pad.stickInfo.deadZoneLeft );
                newC->leftStick.avgY = (newC->leftStick.startY + newC->leftStick.endY) / 2;

                newC->rightStick.startX = oldC->rightStick.endX;
                newC->rightStick.endX = PS4ProcessStickValue( data.rightStick.x, info.pad.stickInfo.deadZoneRight );
                newC->rightStick.avgX = (newC->rightStick.startX + newC->rightStick.endX) / 2;

                newC->rightStick.startY = oldC->rightStick.endY;
                newC->rightStick.endY = PS4ProcessStickValue( data.rightStick.y, info.pad.stickInfo.deadZoneRight );
                newC->rightStick.avgY = (newC->rightStick.startY + newC->rightStick.endY) / 2;

                newC->leftTriggerValue = data.analogButtons.l2 / 255.f;
                newC->rightTriggerValue = data.analogButtons.r2 / 255.f;

                newC->dUp.endedDown = data.buttons & SCE_PAD_BUTTON_UP;
                newC->dDown.endedDown = data.buttons & SCE_PAD_BUTTON_DOWN;
                newC->dLeft.endedDown = data.buttons & SCE_PAD_BUTTON_LEFT;
                newC->dRight.endedDown = data.buttons & SCE_PAD_BUTTON_RIGHT;

                newC->aButton.endedDown = data.buttons & SCE_PAD_BUTTON_CROSS;
                newC->bButton.endedDown = data.buttons & SCE_PAD_BUTTON_CIRCLE;
                newC->xButton.endedDown = data.buttons & SCE_PAD_BUTTON_SQUARE;
                newC->yButton.endedDown = data.buttons & SCE_PAD_BUTTON_TRIANGLE;

                newC->leftThumb.endedDown = data.buttons & SCE_PAD_BUTTON_L3;
                newC->rightThumb.endedDown = data.buttons & SCE_PAD_BUTTON_R3;
                newC->leftShoulder.endedDown = data.buttons & SCE_PAD_BUTTON_L1;
                newC->rightShoulder.endedDown = data.buttons & SCE_PAD_BUTTON_R1;

                newC->start.endedDown = data.buttons & SCE_PAD_BUTTON_TOUCH_PAD;
                newC->options.endedDown = data.buttons & SCE_PAD_BUTTON_OPTIONS;
            }
        }
    }
}

internal r32
PS4GetSecondsElapsed( u64 previousTime, u64* output = nullptr )
{
    u64 procTime = sceKernelGetProcessTime();
    r32 result = (r32)((procTime - previousTime) / 1000000.0);

    if( output )
        *output = procTime;

    return result;
}


int main( int argc, const char* argv[] )
{
    // Init global platform
    globalPlatform = {};
    globalPlatform.Log = PS4Log;
    globalPlatform.DEBUGReadEntireFile = DEBUGPS4ReadEntireFile;
    globalPlatform.DEBUGFreeFileMemory = DEBUGPS4FreeFileMemory;
//     globalPlatform.DEBUGWriteEntireFile = DEBUGPS4WriteEntireFile;
    globalPlatform.AddNewJob = PS4AddNewJob;
    globalPlatform.CompleteAllJobs = PS4CompleteAllJobs;
    globalPlatform.AllocateTexture = PS4AllocateTexture;
    globalPlatform.DeallocateTexture = PS4DeallocateTexture;

    const u32 workerThreadsCount = 6 - 1;       // Reserve one core for the main thread
    PS4WorkerThreadContext threadContexts[workerThreadsCount];

    PS4InitJobQueue( &globalPlatformState.hiPriorityQueue, threadContexts, workerThreadsCount );
    globalPlatform.hiPriorityQueue = &globalPlatformState.hiPriorityQueue;
    globalPlatform.workerThreadsCount = workerThreadsCount;


    globalPlatformState = {};
    PS4ResolvePaths( &globalPlatformState );

    LOG( "Initializing PS4 platform with game DLL at: %s", globalPlatformState.binariesPath );
    GameMemory gameMemory = {};
    gameMemory.platformAPI = &globalPlatform;
    gameMemory.permanentStorageSize = GIGABYTES(2);
    gameMemory.transientStorageSize = GIGABYTES(1);
#if !RELEASE
    gameMemory.debugStorageSize = MEGABYTES(64);
#endif

    void* baseAddress = 0;
#if !RELEASE
    baseAddress = (void*)GIGABYTES(64);
#endif

    // Allocate game memory pools
    u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize + gameMemory.debugStorageSize;
    globalPlatformState.gameMemoryBlock = PS4AllocAndMap( baseAddress, totalSize, SCE_KERNEL_WB_ONION, 0, SCE_KERNEL_PROT_CPU_RW );
    globalPlatformState.gameMemorySize = totalSize;

    gameMemory.permanentStorage = globalPlatformState.gameMemoryBlock;
    gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;
#if !RELEASE
    gameMemory.debugStorage = (u8*)gameMemory.transientStorage + gameMemory.transientStorageSize;
#endif

    //i16 *soundSamples = (i16 *)PS4AllocAndMap( 0, audioOutput.bufferSizeFrames * audioOutput.bytesPerFrame,
                                               //SCE_KERNEL_WB_ONION );

    if( gameMemory.permanentStorage ) //&& soundSamples )
    {
        LOG( ".Allocated game memory with base address: %p", baseAddress );

        sz memoryBytes;
        sceKernelAvailableFlexibleMemorySize( &memoryBytes );
        LOG( ".%d bytes of flexible memory available", memoryBytes );

        char gameCodePath[MAX_PATH];
        const char* filename = "robotrider.prx";
        PS4BuildAbsolutePath( filename, PS4Path::Binary, gameCodePath );
        globalPlatformState.gameCode = PS4LoadGameCode( gameCodePath, &gameMemory );

        // NOTE For some reason, this cannot be static (global) or everything explodes
        PS4RendererState rendererState = PS4InitRenderer();
        globalPlatformState.renderer = &rendererState;

        gameMemory.imGuiContext = PS4InitImGui();

        PS4InitInput( &globalPlatformState );
        GameInput input[2] = {};
        GameInput *newInput = &input[0];
        GameInput *oldInput = &input[1];

        u32 runningFrameCounter = 0;
        // Assume our target for the first frame
        r32 frameElapsedSeconds = 1.0f / targetFramerateHz;
        r32 totalElapsedSeconds = 0;

        u64 startTick = {};
        PS4GetSecondsElapsed( startTick, &startTick );
        u64 frameTick = startTick;

        // Main loop
        globalPlatformState.running = true;
        while( globalPlatformState.running )
        {
            PS4PrepareInputData( &oldInput, &newInput, frameElapsedSeconds, totalElapsedSeconds, 1 );
            PS4ReadControllerStates( &globalPlatformState, oldInput, newInput );
            
            // TODO 
#if 0 // !RELEASE
            // Check for game code updates
            if( GameCodeUpdated( &globalPlatformState ) )
            {
                PS4CompleteAllJobs( globalPlatform.hiPriorityQueue );

                LOG( "Detected updated game code. Reloading.." );
                PS4UnloadGameCode( &globalPlatformState.gameCode );
                globalPlatformState.gameCode
                    = PS4LoadGameCode( gameCodePath, &gameMemory );
                newInput->executableReloaded = true;
            }

            // Check for asset updates
            PS4CheckAssetUpdates( &globalPlatformState );
#endif

            // Setup remaining stuff for the ImGui frame
            ImGuiIO &io = ImGui::GetIO();
            io.DisplaySize.x = kDisplayBufferWidth;
            io.DisplaySize.y = kDisplayBufferHeight;
            ImGui::NewFrame();

            // Prepare audio & video buffers
            GameAudioBuffer audioBuffer = {};
            //audioBuffer.samplesPerSecond = audioOutput.samplingRate;
            //audioBuffer.channelCount = AUDIO_CHANNELS;
            //audioBuffer.frameCount = audioFramesToWrite;
            //audioBuffer.samples = soundSamples;

#if 1
            ResetRenderCommands( &rendererState.renderCommands );

            // Ask the game to render one frame
            globalPlatformState.gameCode.UpdateAndRender( &gameMemory, newInput,
                                                          &rendererState.renderCommands, &audioBuffer );
#endif

            // Display frame
            PS4RenderToOutput( rendererState.renderCommands, &rendererState, &gameMemory );
            PS4RenderImGui();
            PS4SwapBuffers( &rendererState );

            frameElapsedSeconds = PS4GetSecondsElapsed( frameTick, &frameTick );
            totalElapsedSeconds = PS4GetSecondsElapsed( startTick );
            ++runningFrameCounter;
        }

        PS4ShutdownRenderer( &rendererState );
    }
    else
    {
        LOG( ".ERROR: Memory allocation failed!" );
    }

    return 0;
}
