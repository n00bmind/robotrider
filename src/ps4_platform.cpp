#include "game.h"

#include <stdio.h>
#include <string.h>
#include <kernel.h>

#include "ps4_platform.h"



PlatformAPI globalPlatform;
internal PS4State globalPlatformState;




internal void*
PS4AllocAndMap( void* address, sz size, int memoryType,
                int protection = SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_RW )
{
    ASSERT( size > 0 );

    const u32 pageSize = 16*1024;
    size = ((size + pageSize - 1) / pageSize) * pageSize;

    off_t physicalOffset = 0;
    i32 ret = sceKernelAllocateMainDirectMemory( size, 0, memoryType, &physicalOffset );
    ASSERT( ret == 0 );

    // NOTE Never specify 0 for the 'protection' field! Non-consistent page faults will ensue!
    ret = sceKernelMapDirectMemory( &address, size, protection, address ? SCE_KERNEL_MAP_FIXED : 0, physicalOffset, 0 );
    ASSERT( ret == 0 );

    return address;
}

internal void
PS4Free( void* address )
{
    // TODO Need to track physical memory offset ourselves!
    sz physicalOffset = 0;
    sz size = 0;

    i32 ret = sceKernelReleaseDirectMemory( physicalOffset, size );
    ASSERT( ret == 0 );
}

internal sz
PS4JoinPaths( const char* pathBase, const char* relativePath, char* destination, bool addTrailingSlash = false )
{
    const char* format = "%s/%s";
    if( destination[0] == '/' )
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
    snprintf( platformState->assetDataPath, MAX_PATH, "/app0/data" );
}

PLATFORM_LOG(PS4Log)
{
    // TODO 
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
        // If path is relative, use executable location to complete it
        PS4JoinPaths( globalPlatformState.assetDataPath, filename, absolutePath );
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
            result.contents = PS4AllocAndMap( 0, fileSize + 1, SCE_KERNEL_WB_ONION, SCE_KERNEL_PROT_CPU_RW );

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
        LOG( "ERROR: Failed opening file for reading (%#x)", fileHandle );
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

PLATFORM_ALLOCATE_TEXTURE(PS4AllocateTexture)
{
    // TODO
    return nullptr;
}

PLATFORM_DEALLOCATE_TEXTURE(PS4DeallocateTexture)
{
    // TODO
}

internal PS4GameCode
PS4LoadGameCode( const char* prxPath, GameMemory* gameMemory )
{
    PS4GameCode result = {};
    
//     SceKernelStat bla = {};
//     if( sceKernelStat( prxPath, &bla ) == 0 )
//         result.lastPrxWriteTime = bla.st_mtim;

    // TODO Check if we should specify '/host' for the root path here
    result.gameCodePrx = sceKernelLoadStartModule( prxPath, 0, nullptr, 0, nullptr, nullptr );
    if( result.gameCodePrx > 0 )
    {
        sceKernelDlsym( result.gameCodePrx, "GameSetupAfterReload", (void**)&result.SetupAfterReload );
        sceKernelDlsym( result.gameCodePrx, "GameUpdateAndRender", (void**)&result.UpdateAndRender );
        sceKernelDlsym( result.gameCodePrx, "GameLogCallback", (void**)&result.LogCallback );
        sceKernelDlsym( result.gameCodePrx, "DebugGameFrameEnd", (void**)&result.DebugFrameEnd );

        result.isValid = result.UpdateAndRender != nullptr;
    }

//     if( result.SetupAfterReload )
//         result.SetupAfterReload( gameMemory );

    if( !result.UpdateAndRender )
        result.UpdateAndRender = GameUpdateAndRenderStub;

    return result;
}

int main( int argc, const char* argv[] )
{
    // Init global platform
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

    globalPlatformState.renderer = Renderer::Gnmx;
    PS4ResolvePaths( &globalPlatformState );

    LOG( "Initializing PS4 platform with game DLL at: %s", globalPlatformState.sourceDLLPath );
    GameMemory gameMemory = {};
    gameMemory.platformAPI = &globalPlatform;
    gameMemory.permanentStorageSize = GIGABYTES(2);
    gameMemory.transientStorageSize = GIGABYTES(1);
#if DEBUG
    gameMemory.debugStorageSize = MEGABYTES(64);
#endif


    // TODO Decide a proper size for this
    u32 renderBufferSize = MEGABYTES( 4 );
    u8 *renderBuffer = (u8 *)PS4AllocAndMap( 0, renderBufferSize, 0, SCE_KERNEL_WC_GARLIC );
    u32 vertexBufferSize = 1024*1024;
    TexturedVertex *vertexBuffer = (TexturedVertex *)PS4AllocAndMap( 0, vertexBufferSize * sizeof(TexturedVertex),
                                                                     0, SCE_KERNEL_WC_GARLIC );
    u32 indexBufferSize = vertexBufferSize * 8;
    u32 *indexBuffer = (u32 *)PS4AllocAndMap( 0, indexBufferSize * sizeof(u32), 0, SCE_KERNEL_WC_GARLIC );

    RenderCommands renderCommands = InitRenderCommands( renderBuffer, renderBufferSize,
                                                        vertexBuffer, vertexBufferSize,
                                                        indexBuffer, indexBufferSize );

    void* baseAddress = 0;
#if DEBUG
    baseAddress = (void*)GIGABYTES(64);
#endif

    // Allocate game memory pools
    u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize + gameMemory.debugStorageSize;
    globalPlatformState.gameMemoryBlock = PS4AllocAndMap( baseAddress, totalSize, SCE_KERNEL_WB_ONION, SCE_KERNEL_PROT_CPU_RW );
    globalPlatformState.gameMemorySize = totalSize;

    gameMemory.permanentStorage = globalPlatformState.gameMemoryBlock;
    gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;
#if DEBUG
    gameMemory.debugStorage = (u8*)gameMemory.transientStorage + gameMemory.transientStorageSize;
#endif

    //i16 *soundSamples = (i16 *)PS4AllocAndMap( 0, audioOutput.bufferSizeFrames * audioOutput.bytesPerFrame,
                                               //0, SCE_KERNEL_WB_ONION );

    if( gameMemory.permanentStorage && renderCommands.isValid ) //&& soundSamples )
    {
//         LOG( ".Allocated game memory with base address: %p", baseAddress );

        globalPlatformState.gameCode = PS4LoadGameCode( "/hostapp/robotrider.prx", &gameMemory );

        GameInput input[2] = {};
        GameInput *newInput = &input[0];
        GameInput *oldInput = &input[1];

        // Main loop
        globalPlatformState.running = true;
        while( globalPlatformState.running )
        {
            //PS4PrepareInputData( oldInput, newInput,
            //lastDeltaTimeSecs, totalElapsedSeconds, runningFrameCounter );

            // Process input

            // Prepare audio & video buffers
            GameAudioBuffer audioBuffer = {};
            //audioBuffer.samplesPerSecond = audioOutput.samplingRate;
            //audioBuffer.channelCount = AUDIO_CHANNELS;
            //audioBuffer.frameCount = audioFramesToWrite;
            //audioBuffer.samples = soundSamples;

            ResetRenderCommands( &renderCommands );


            // Ask the game to render one frame
            globalPlatformState.gameCode.UpdateAndRender( &gameMemory, newInput, &renderCommands, &audioBuffer );

        }
    }
    else
    {
        LOG( ".ERROR: Memory allocation failed!" );
    }

    return 0;
}