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

#pragma warning( push )
#pragma warning( disable: 4365 )
#pragma warning( disable: 4774 )

#include <windows.h>

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_widgets.cpp"

#include <gl/gl.h>
#include "glext.h"
#include "wglext.h"

#include <xinput.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <shlwapi.h>
#include <stdio.h>

#pragma warning( pop )

#include "opengl_renderer.h"
#include "opengl_renderer.cpp"

// Prefer discrete graphics where available
extern "C"
{
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x01;
    __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x01;
}

#define VIDEO_TARGET_FRAMERATE 60
#define AUDIO_BITDEPTH 16
#define AUDIO_CHANNELS 2
#define AUDIO_LATENCY_SAMPLES 2200

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

#include "win32_platform.h"


PlatformAPI globalPlatform;

internal OpenGLState globalOpenGLState;
internal Win32State globalPlatformState;


// FIXME These probably don't need to be globals at all.. PRUNE!
internal bool globalRunning;
internal i32 globalMonitorRefreshHz;
internal IAudioClient* globalAudioClient;
internal IAudioRenderClient* globalAudioRenderClient;
internal i64 globalPerfCounterFrequency;
#if !RELEASE
internal HCURSOR DEBUGglobalCursor;
#endif


// Global switches
internal bool globalVSyncEnabled = false;



DEBUG_PLATFORM_JOIN_PATHS(Win32JoinPaths)
{
    char buffer[PLATFORM_PATH_MAX] = {};

    sz len = Min( strlen( root ), (sz)PLATFORM_PATH_MAX );
    strncpy( buffer, root, len );

    // Add separator if root path is not empty and there's none at either of the two
    if( len > 0 && len < PLATFORM_PATH_MAX )
    {
        if( path[0] != '\\' && path[0] != '/' &&
            buffer[len-1] != '\\' && buffer[len-1] != '/' )
        {
            buffer[len++] = '/';
        }
    }

    len = PLATFORM_PATH_MAX - len;
    strncat( buffer, path, len );
    
    if( canonicalize )
        GetFullPathName( buffer, PLATFORM_PATH_MAX, destination, nullptr );
    else
        strncpy( destination, buffer, PLATFORM_PATH_MAX );

    return strlen( destination );
}

internal const char*
FindFilename( const char* path )
{
    const char *onePastLastSlash = path;
    for( const char *scanChar = path; *scanChar; ++scanChar )
    {
        if( *scanChar == '\\' || *scanChar == '/' )
        {
            onePastLastSlash = scanChar + 1;
        }
    }

    return onePastLastSlash;
}

DEBUG_PLATFORM_GET_PARENT_PATH(Win32GetParentPath)
{
    const char *pathSeparator = FindFilename( path );
    if( pathSeparator != path )
        --pathSeparator;

    sz len = Sz( pathSeparator - path );
    if( destination != path )
        strncpy( destination, path, len );

    destination[len] = 0;
}

internal char *
ExtractFileExtension( char *filename, char *destination, sz destinationMaxLen )
{
    char *lastDot = filename;
    for( char *scanChar = filename; *scanChar; ++scanChar )
    {
        if( *scanChar == '.' )
        {
            lastDot = scanChar;
        }
    }
    if( lastDot != filename && destination )
    {
        strncpy( destination, lastDot + 1, destinationMaxLen );
    }

    return lastDot;
}

internal void
TrimFileExtension( char* filename )
{
    char* lastDotPos = ExtractFileExtension( filename, nullptr, 0 );
    *lastDotPos = 0;
}


internal bool
AcceptAssetFileData( const WIN32_FIND_DATA& findData )
{
    bool result = true;
    String filename( (char*)findData.cFileName );

    if( filename.IsEqual( "." ) || filename.IsEqual( ".." ) )
        result = false;

    return result;
}

DEBUG_PLATFORM_LIST_ALL_ASSETS(Win32ListAllAssets)
{
    DEBUGFileInfoList result = {};

    char joinedPath[PLATFORM_PATH_MAX] = {};
    Win32JoinPaths( globalPlatformState.dataFolderPath, relativeRootPath, joinedPath, true );
    char findPath[PLATFORM_PATH_MAX] = {};
    Win32JoinPaths( joinedPath, "*", findPath, false );

    WIN32_FIND_DATA findData = {};
    HANDLE hFind = FindFirstFile( findPath, &findData );
    if( hFind != INVALID_HANDLE_VALUE )
    {
        do
        {
            if( AcceptAssetFileData( findData ) )
                ++result.entryCount;
        } while( FindNextFile( hFind, &findData ) );
        FindClose( hFind );
    }

    result.files = PUSH_ARRAY( arena, DEBUGFileInfo, result.entryCount );
    hFind = FindFirstFile( findPath, &findData );
    if( hFind != INVALID_HANDLE_VALUE )
    {
        int i = 0;
        do
        {
            if( AcceptAssetFileData( findData ) )
            {
                DEBUGFileInfo* info = result.files + i++;
                *info = {};
                Win32JoinPaths( joinedPath, findData.cFileName, info->fullPath, false );
                info->name = FindFilename( info->fullPath );
                info->size = ((sz)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
                info->lastUpdated = ((u64)findData.ftLastWriteTime.dwHighDateTime << 32) | findData.ftLastWriteTime.dwLowDateTime;
                info->isFolder = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }

        } while( FindNextFile( hFind, &findData ) );
        FindClose( hFind );
    }

    return result;
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGWin32FreeFileMemory)
{
    if( memory )
    {
        VirtualFree( memory, 0, MEM_RELEASE );
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGWin32ReadEntireFile)
{
    DEBUGReadFileResult result = {};

    char absolutePath[PLATFORM_PATH_MAX];
    if( PathIsRelative( filename ) )
    {
        // If path is relative, use data location to complete it
        Win32JoinPaths( globalPlatformState.dataFolderPath, filename, absolutePath, true );
    }

    HANDLE fileHandle = CreateFile( absolutePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    if( fileHandle != INVALID_HANDLE_VALUE )
    {
        LARGE_INTEGER fileSize;
        if( GetFileSizeEx( fileHandle, &fileSize ) )
        {
            u32 fileSize32 = U32( (u64)fileSize.QuadPart );
            result.contents = VirtualAlloc( 0, fileSize32 + 1, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            if( result.contents )
            {
                DWORD bytesRead;
                if( ReadFile( fileHandle, result.contents, fileSize32, &bytesRead, 0 )
                    && (fileSize32 == bytesRead) )
                {
                    // Null-terminate to help when handling text files
                    *((u8 *)result.contents + fileSize32) = '\0';
                    result.contentSize = I32( fileSize32 + 1 );
                }
                else
                {
                    LOG( "ERROR: ReadFile failed for '%s'", filename );
                    DEBUGWin32FreeFileMemory( result.contents );
                    result.contents = 0;
                }
            }
            else
            {
                LOG( "ERROR: Couldn't allocate buffer for file contents" );
            }
        }
        else
        {
            LOG( "ERROR: Failed querying file size for '%s'", filename );
        }

        CloseHandle( fileHandle );
    }
    else
    {
        LOG( "ERROR: Failed opening file '%s' for reading", filename );
    }

    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGWin32WriteEntireFile)
{
    bool result = false;

    char absolutePath[PLATFORM_PATH_MAX];
    if( PathIsRelative( filename ) )
    {
        // If path is relative, use data location to complete it
        Win32JoinPaths( globalPlatformState.dataFolderPath, filename, absolutePath, true );
        filename = absolutePath;
    }

    HANDLE fileHandle = CreateFile( filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0 );
    if( fileHandle != INVALID_HANDLE_VALUE )
    {
        DWORD bytesWritten;
        if( WriteFile( fileHandle, memory, memorySize, &bytesWritten, 0 ) )
        {
            result = (bytesWritten == memorySize);
        }
        else
        {
            LOG( "ERROR: WriteFile failed" );
        }

        CloseHandle( fileHandle );
    }
    else
    {
        LOG( "ERROR: Failed opening file for writing" );
    }

    return result;
}


// TODO Cache all platform logs in some buffer and bulk dump them to game console when it's first available
// Another solution could be externalizing the console entry buffer to the platform?
PLATFORM_LOG(Win32Log)
{
    char buffer[1024];

    va_list args;
    va_start( args, fmt );
    vsnprintf( buffer, ARRAYCOUNT(buffer), fmt, args );
    va_end( args );

    printf( "%s\n", buffer );

    if( globalPlatformState.gameCode.LogCallback )
        globalPlatformState.gameCode.LogCallback( buffer );
}


inline FILETIME
Win32GetLastWriteTime( char *filename )
{
    FILETIME lastWriteTime = {};
    WIN32_FILE_ATTRIBUTE_DATA data;

    if( GetFileAttributesEx( filename, GetFileExInfoStandard, &data ) )
    {
        lastWriteTime = data.ftLastWriteTime;
    }

    return lastWriteTime;
}

internal Win32GameCode
Win32LoadGameCode( char *sourceDLLPath, char *tempDLLPath, GameMemory *gameMemory )
{
    Win32GameCode result = {};
    result.lastDLLWriteTime = Win32GetLastWriteTime( sourceDLLPath );

    BOOL res = CopyFile( sourceDLLPath, tempDLLPath, FALSE );
    if( !res )
    {
        DWORD err = GetLastError();
        LOG( "ERROR: DLL copy to temp location failed! (%08x)", err );
        INVALID_CODE_PATH
    }

    result.gameCodeDLL = LoadLibrary( tempDLLPath );
    if( result.gameCodeDLL )
    {
        // Cast to void* first to avoid MSVC whining
        result.SetupAfterReload = (GameSetupAfterReloadFunc *)(void*)GetProcAddress( result.gameCodeDLL, "GameSetupAfterReload" );
        result.UpdateAndRender = (GameUpdateAndRenderFunc *)(void*)GetProcAddress( result.gameCodeDLL, "GameUpdateAndRender" );
        result.LogCallback = (GameLogCallbackFunc *)(void*)GetProcAddress( result.gameCodeDLL, "GameLogCallback" );
        result.DebugFrameEnd = (DebugGameFrameEndFunc *)(void*)GetProcAddress( result.gameCodeDLL, "DebugGameFrameEnd" );

        result.isValid = result.UpdateAndRender != 0;
    }

    if( result.SetupAfterReload )
        result.SetupAfterReload( gameMemory );

    if( !result.UpdateAndRender )
        result.UpdateAndRender = GameUpdateAndRenderStub;

    return result;
}

internal void
Win32UnloadGameCode( Win32GameCode *gameCode )
{
    if( gameCode->gameCodeDLL )
    {
        FreeLibrary( gameCode->gameCodeDLL );
    }

    *gameCode = {0};
    gameCode->UpdateAndRender = GameUpdateAndRenderStub;
}

internal void
Win32SetupAssetUpdateListeners( Win32State *platformState )
{
    for( int i = 0; i < platformState->assetListenerCount; ++i )
    {
        Win32AssetUpdateListener& listener = platformState->assetListeners[i];

        char absolutePath[PLATFORM_PATH_MAX];
        Win32JoinPaths( platformState->dataFolderPath, listener.relativePath, absolutePath, true );

        listener.dirHandle
            = CreateFile( absolutePath, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                          OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL );

        if( listener.dirHandle == INVALID_HANDLE_VALUE )
        {
            LOG( "ERROR :: Could not open directory for reading (%s)", absolutePath );
            continue;
        }

        listener.overlapped = {0};
        listener.overlapped.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

        BOOL read = ReadDirectoryChangesW( listener.dirHandle,
                                           platformState->assetsNotifyBuffer,
                                           sizeof(platformState->assetsNotifyBuffer),
                                           TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE,
                                           NULL, &listener.overlapped, NULL );
        ASSERT( read );
    }
}

internal void
Win32CheckAssetUpdates( Win32State *platformState )
{
    for( int i = 0; i < platformState->assetListenerCount; ++i )
    {
        Win32AssetUpdateListener& listener = platformState->assetListeners[i];

        DWORD bytesReturned;
        // Return immediately if no info ready
        BOOL result = GetOverlappedResult( listener.dirHandle,
                                           &listener.overlapped,
                                           &bytesReturned, FALSE );

        if( result )
        {
            ASSERT( bytesReturned < sizeof(platformState->assetsNotifyBuffer) );

            u32 entryOffset = 0;
            do
            {
                FILE_NOTIFY_INFORMATION *notifyInfo
                    = (FILE_NOTIFY_INFORMATION *)(platformState->assetsNotifyBuffer + entryOffset);
                if( notifyInfo->Action == FILE_ACTION_MODIFIED )
                {
                    char filename[PLATFORM_PATH_MAX];
                    WideCharToMultiByte( CP_ACP, WC_COMPOSITECHECK,
                                         notifyInfo->FileName, int( notifyInfo->FileNameLength ),
                                         filename, PLATFORM_PATH_MAX, NULL, NULL );
                    filename[notifyInfo->FileNameLength/2] = 0;

                    // Check and strip extension
                    char extension[16];
                    ExtractFileExtension( filename, extension, ARRAYCOUNT(extension) );

                    if( strcmp( extension, listener.extension ) == 0 )
                    {
                        LOG( "Shader file '%s' was modified. Reloading..", filename );

                        char assetPath[PLATFORM_PATH_MAX];
                        Win32JoinPaths( SHADERS_RELATIVE_PATH, filename, assetPath, false );

                        DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( assetPath );
                        ASSERT( read.contents );
                        listener.callback( filename, read );
                        globalPlatform.DEBUGFreeFileMemory( read.contents );
                    }
                }

                entryOffset = notifyInfo->NextEntryOffset;
            } while( entryOffset );

            // Restart monitorization
            BOOL read = ReadDirectoryChangesW( listener.dirHandle,
                                               platformState->assetsNotifyBuffer,
                                               sizeof(platformState->assetsNotifyBuffer),
                                               TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE,
                                               NULL, &listener.overlapped, NULL );
            ASSERT( read );
        }
        else
        {
            ASSERT( GetLastError() == ERROR_IO_INCOMPLETE );
        }
    }
}

// XInput function pointers and stubs
#define XINPUT_GET_STATE(name) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_STATE *pState )
typedef XINPUT_GET_STATE(XInputGetStateFunc);
XINPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
internal XInputGetStateFunc *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define XINPUT_SET_STATE(name) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_VIBRATION* pVibration )
typedef XINPUT_SET_STATE(XInputSetStateFunc);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
internal XInputSetStateFunc *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32InitXInput()
{
    HMODULE XInputLibrary = LoadLibrary( "xinput1_4.dll" );
    if( !XInputLibrary )
    {
        LOG( ".WARNING: xinput1_4.dll couldn't be loaded. Using xinput1_3.dll" );
        XInputLibrary = LoadLibrary( "xinput1_3.dll" );
    }

    if( XInputLibrary )
    {
        XInputGetState = (XInputGetStateFunc *)(void*)GetProcAddress( XInputLibrary, "XInputGetState" );
        if( !XInputGetState ) { XInputGetState = XInputGetStateStub; }

        XInputSetState = (XInputSetStateFunc *)(void*)GetProcAddress( XInputLibrary, "XInputSetState" );
        if( !XInputSetState ) { XInputSetState = XInputSetStateStub; }
    }
    else
    {
        LOG( ".ERROR: Couldn't load xinput1_3.dll!" );
    }
}


// WASAPI functions
internal Win32AudioOutput
Win32InitWASAPI( u32 samplingRate, u16 bitDepth, u16 channelCount, i32 bufferSizeFrames )
{
    LOG( ".Initializing WASAPI buffer..." );

    Win32AudioOutput audioOutput = {};

    if( FAILED(CoInitializeEx( 0, COINIT_SPEED_OVER_MEMORY )) )
    {
        LOG( ".ERROR: CoInitializeEx failed!" );
        ASSERT( false );
        return audioOutput;
    }
    
    IMMDeviceEnumerator *enumerator;
    if( FAILED(CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator) )) )
    {
        LOG( ".ERROR: CoCreateInstance failed!" );
        ASSERT( false );
        return audioOutput;
    }

    IMMDevice *device;
    if( FAILED(enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device )) )
    {
        LOG( ".ERROR: Couldn't get default audio endpoint!" );
        ASSERT( false );
        return audioOutput;
    }

    if( FAILED(device->Activate( __uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID *)&globalAudioClient )) )
    {
        LOG( ".ERROR: Couldn't activate IMMDevice" );
        ASSERT( false );
        return audioOutput;
    }

    WAVEFORMATEXTENSIBLE waveFormat;
    waveFormat.Format.cbSize = sizeof(waveFormat);
    waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    waveFormat.Format.nChannels = channelCount;
    waveFormat.Format.nSamplesPerSec = samplingRate;
    waveFormat.Format.wBitsPerSample = bitDepth;
    waveFormat.Format.nBlockAlign = (WORD)(waveFormat.Format.nChannels * waveFormat.Format.wBitsPerSample / 8);
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Samples.wValidBitsPerSample = bitDepth;
    waveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    waveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    // TODO Use EVENTCALLBACK mode because this seems to improve latency
    REFERENCE_TIME bufferDuration = 10000000LL * bufferSizeFrames / samplingRate;     // buffer size in hundreds of nanoseconds (1 sec.)
    HRESULT result = globalAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED,
                                                    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                                    bufferDuration, 0, (WAVEFORMATEX *)&waveFormat, nullptr );
#if 0
    // Try to initialize again using the device preferred sample rate
	if( result != S_OK )
    {
        // Discard partially initialized IAudioClient because some devices will fail otherwise
        ASSERT( SUCCEEDED(device->Activate( __uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID *)&globalAudioClient )) );
        
        WAVEFORMATEXTENSIBLE* mixFormat;
        globalAudioClient->GetMixFormat( (WAVEFORMATEX **)&mixFormat );

        waveFormat.Format.nSamplesPerSec = mixFormat->Format.nSamplesPerSec;
        waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
        result = globalAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST,
                                                bufferDuration, 0, (WAVEFORMATEX *)&waveFormat, nullptr );
        // TODO Diagnostic
        ASSERT( result == S_OK );
    }
#endif

    if( result != S_OK )
    {
        LOG( ".ERROR: Couldn't initialize global audio client" );
        ASSERT( false );
        return audioOutput;
    }

    if( FAILED(globalAudioClient->GetService( IID_PPV_ARGS(&globalAudioRenderClient) )) )
    {
        LOG( ".ERROR: Couldn't get audio render client" );
        ASSERT( false );
        return audioOutput;
    }

    u32 audioFramesCount;
    if( FAILED(globalAudioClient->GetBufferSize( &audioFramesCount )) )
    {
        LOG( ".ERROR: Failed querying buffer size" );
        ASSERT( false );
        return audioOutput;
    }

    ASSERT( U32( bufferSizeFrames ) == audioFramesCount );

    audioOutput.samplingRate = waveFormat.Format.nSamplesPerSec;
    audioOutput.bytesPerFrame = AUDIO_BITDEPTH * AUDIO_CHANNELS / 8;
    audioOutput.bufferSizeFrames = bufferSizeFrames;
    audioOutput.runningFrameCount = 0;

    return audioOutput;
}

internal void
Win32BlitAudioBuffer( GameAudioBuffer *sourceBuffer, int framesToWrite, Win32AudioOutput *audioOutput )
{
    BYTE *audioData;
    HRESULT hr = globalAudioRenderClient->GetBuffer( U32( framesToWrite ), &audioData );
    if( hr == S_OK )
    {
        i16* sourceSample = sourceBuffer->samples;
        i16* destSample = (i16*)audioData;
        for( int frameIndex = 0; frameIndex < framesToWrite; ++frameIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++audioOutput->runningFrameCount;
        }

        globalAudioRenderClient->ReleaseBuffer( U32( framesToWrite ), 0 );
    }
    else
    {
        ASSERT( false && hr );
    }
}

internal ImGuiContext *
Win32InitImGui( HWND window )
{
    ImGuiContext *result = OpenGLInitImGui( globalOpenGLState );

    ImGuiIO& io = ImGui::GetIO();
    // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array
    // that we will update during the application lifetime.
    io.KeyMap[ImGuiKey_Tab] = VK_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
    io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
    io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
    io.KeyMap[ImGuiKey_Home] = VK_HOME;
    io.KeyMap[ImGuiKey_End] = VK_END;
    io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
    io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
    io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
    io.KeyMap[ImGuiKey_A] = 'A';
    io.KeyMap[ImGuiKey_C] = 'C';
    io.KeyMap[ImGuiKey_V] = 'V';
    io.KeyMap[ImGuiKey_X] = 'X';
    io.KeyMap[ImGuiKey_Y] = 'Y';
    io.KeyMap[ImGuiKey_Z] = 'Z';

    io.ImeWindowHandle = window;

    return result;
}


internal Win32WindowDimension
Win32GetWindowDimension( HWND window )
{
    Win32WindowDimension result;

    RECT clientRect;
    GetClientRect( window, &clientRect );
    result.width = clientRect.right - clientRect.left;
    result.height = clientRect.bottom - clientRect.top;

    return result;
}

internal void
Win32AllocateBackBuffer( Win32OffscreenBuffer *buffer, int width, int height )
{
    if( buffer->memory )
    {
        VirtualFree( buffer->memory, 0, MEM_RELEASE );
    }
    buffer->width = width;
    buffer->height = height;
    buffer->bytesPerPixel = 4;

    buffer->bitmapInfo.bmiHeader.biSize = sizeof(buffer->bitmapInfo.bmiHeader);
    buffer->bitmapInfo.bmiHeader.biWidth = width;
    buffer->bitmapInfo.bmiHeader.biHeight = -height;
    buffer->bitmapInfo.bmiHeader.biPlanes = 1;
    buffer->bitmapInfo.bmiHeader.biBitCount = 32;
    buffer->bitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = width * height * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc( 0, Sz( bitmapMemorySize ), MEM_COMMIT, PAGE_READWRITE );
}

internal void
Win32DisplayInWindow( const Win32State& platformState, const RenderCommands &commands,
                      HDC deviceContext, int windowWidth, int windowHeight, GameMemory* gameMemory )
{
    switch( platformState.renderer )
    {
        case Renderer::OpenGL:
        {
            OpenGLRenderToOutput( commands, &globalOpenGLState, gameMemory );
            ImGui::Render();
            OpenGLRenderImGui( globalOpenGLState, ImGui::GetDrawData() );
            SwapBuffers( deviceContext );
        } break;

        INVALID_DEFAULT_CASE;
    }
}


internal void
Win32RegisterRawMouseInput( HWND window )
{
#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC         ((USHORT) 0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE        ((USHORT) 0x02)
#endif

    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC; 
    rid[0].usUsage = HID_USAGE_GENERIC_MOUSE; 
    rid[0].dwFlags = 0;
    rid[0].hwndTarget = NULL;
    RegisterRawInputDevices( rid, 1, sizeof(rid[0]) );
}

internal GameControllerInput*
Win32ResetKeyMouseController( GameInput* oldInput, GameInput* newInput )
{
    GameControllerInput *oldKeyMouseController = GetController( oldInput, 0 );
    GameControllerInput *newKeyMouseController = GetController( newInput, 0 );

    *newKeyMouseController = {};
    newKeyMouseController->isConnected = true;

    newKeyMouseController->leftStick = oldKeyMouseController->leftStick;
    //newKeyMouseController->rightStick = oldKeyMouseController->rightStick;

    for( int buttonIndex = 0;
         buttonIndex < ARRAYCOUNT( newKeyMouseController->buttons );
         ++buttonIndex )
    {
        newKeyMouseController->buttons[buttonIndex].endedDown =
            oldKeyMouseController->buttons[buttonIndex].endedDown;
    }

    return newKeyMouseController;
}

internal void
Win32ResetController( GameControllerInput *controller )
{
    *controller = {};
    controller->isConnected = true;
}

internal void
Win32SetButtonState( GameButtonState *newState, bool isDown )
{
    if( newState->endedDown != isDown )
    {
        newState->endedDown = isDown;
        ++newState->halfTransitionCount;
    }
}

internal void
Win32ProcessXInputDigitalButton( DWORD buttonStateBits, GameButtonState *oldState, DWORD buttonBit,
                                 GameButtonState *newState )
{
    newState->endedDown = ((buttonStateBits & buttonBit) == buttonBit);
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

internal f32
Win32ProcessXInputStickValue( SHORT value, SHORT deadZoneThreshold )
{
    // TODO Cube the values as explained in the XInput help
    // to give more sensitivity to the movement
    f32 result = 0;
    // TODO Does hardware have a round deadzone?
    if( value < -deadZoneThreshold )
    {
        result = (f32)(value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if( value > deadZoneThreshold )
    {
        result = (f32)(value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

internal void
Win32ProcessXInputControllers( GameInput* oldInput, GameInput* newInput, GameControllerInput* keyMouseController )
{
    // TODO Don't call GetState on disconnected controllers cause that has a big impact
    // https://hero.handmade.network/forums/code-discussion/t/847-xinputgetstate
    // TODO Should we poll this more frequently?
    // TODO Mouse?
    int maxControllerCount = XUSER_MAX_COUNT;
    // First controller is the keyboard, and we only support Xbox controllers for now
    ASSERT( ARRAYCOUNT(GameInput::_controllers) >= XUSER_MAX_COUNT + 1 );

    for( int index = 1, gamepadIndex = 0; index < ARRAYCOUNT(GameInput::_controllers); ++index, ++gamepadIndex )
    {
        GameControllerInput *oldController = GetController( oldInput, index );
        GameControllerInput *newController = GetController( newInput, index );
        *newController = {};

        if( gamepadIndex < XUSER_MAX_COUNT )
        {
            newController->isConnected = false;

            // Use key-mouse data as the first controller too (can be overriden by the real one)
            // TODO Merge data instead
            if( gamepadIndex == 0 && keyMouseController )
                *newController = *keyMouseController;

            XINPUT_STATE controllerState;
            // FIXME This call apparently stalls for a few hundred thousand cycles when the controller is not present
            if( XInputGetState( U32( gamepadIndex ), &controllerState ) == ERROR_SUCCESS )
            {
                // Plugged in
                newController->isConnected = true;
                XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

                newController->leftStick.startX = oldController->leftStick.endX;
                newController->leftStick.startY = oldController->leftStick.endY;

                newController->leftStick.endX = Win32ProcessXInputStickValue( pad->sThumbLX,
                                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
                newController->leftStick.avgX
                    = (newController->leftStick.startX + newController->leftStick.endX) / 2;

                newController->leftStick.endY = Win32ProcessXInputStickValue( pad->sThumbLY,
                                                                              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE );
                newController->leftStick.avgY
                    = (newController->leftStick.startY + newController->leftStick.endY) / 2;

                newController->rightStick.startX = oldController->rightStick.endX;
                newController->rightStick.startY = oldController->rightStick.endY;

                newController->rightStick.endX = Win32ProcessXInputStickValue( pad->sThumbRX,
                                                                               XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
                newController->rightStick.avgX
                    = (newController->rightStick.startX + newController->rightStick.endX) / 2;

                newController->rightStick.endY = Win32ProcessXInputStickValue( pad->sThumbRY,
                                                                               XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE );
                newController->rightStick.avgY
                    = (newController->rightStick.startY + newController->rightStick.endY) / 2;

                // TODO Left/right triggers

                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->dUp,
                                                 XINPUT_GAMEPAD_DPAD_UP, &newController->dUp );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->dDown,
                                                 XINPUT_GAMEPAD_DPAD_DOWN, &newController->dDown );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->dLeft,
                                                 XINPUT_GAMEPAD_DPAD_LEFT, &newController->dLeft );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->dRight,
                                                 XINPUT_GAMEPAD_DPAD_RIGHT, &newController->dRight );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->aButton,
                                                 XINPUT_GAMEPAD_A, &newController->aButton );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->bButton,
                                                 XINPUT_GAMEPAD_B, &newController->bButton );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->xButton,
                                                 XINPUT_GAMEPAD_X, &newController->xButton );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->yButton,
                                                 XINPUT_GAMEPAD_Y, &newController->yButton );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->leftThumb,
                                                 XINPUT_GAMEPAD_LEFT_THUMB, &newController->leftThumb );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->rightThumb,
                                                 XINPUT_GAMEPAD_RIGHT_THUMB, &newController->rightThumb );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->leftShoulder,
                                                 XINPUT_GAMEPAD_LEFT_SHOULDER, &newController->leftShoulder );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->rightShoulder,
                                                 XINPUT_GAMEPAD_RIGHT_SHOULDER, &newController->rightShoulder );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->start,
                                                 XINPUT_GAMEPAD_START, &newController->start );
                Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->back,
                                                 XINPUT_GAMEPAD_BACK, &newController->back );

#if 1
                // Link left stick and dPad direction buttons
                // FIXME This shouldn't be done at the platform level!
                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                {
                    newController->leftStick.avgY = 1.0f;
                }
                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                {
                    newController->leftStick.avgY = -1.0f;
                }
                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                {
                    newController->leftStick.avgX = -1.0f;
                }
                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                {
                    newController->leftStick.avgX = 1.0f;
                }

                f32 threshold = 0.5f;
                Win32ProcessXInputDigitalButton( (newController->leftStick.avgX < -threshold) ? 1u : 0u,
                                                 &oldController->dLeft, 1u,
                                                 &newController->dLeft );
                Win32ProcessXInputDigitalButton( (newController->leftStick.avgX > threshold) ? 1u : 0u,
                                                 &oldController->dRight, 1u,
                                                 &newController->dRight );
                Win32ProcessXInputDigitalButton( (newController->leftStick.avgY < -threshold) ? 1u : 0u,
                                                 &oldController->dDown, 1u,
                                                 &newController->dDown );
                Win32ProcessXInputDigitalButton( (newController->leftStick.avgY > threshold) ? 1u : 0u,
                                                 &oldController->dUp, 1u,
                                                 &newController->dUp );
#endif
            }
        }
    }
}

internal void
Win32PrepareInputData( GameInput** oldInput, GameInput** newInput, float elapsedSeconds, float totalSeconds, u32 frameCounter )
{
    // Swap pointers
    GameInput *temp = *newInput;
    *newInput = *oldInput;
    *oldInput = temp;

    (*newInput)->gameCodeReloaded = false;
    (*newInput)->frameElapsedSeconds = elapsedSeconds;
    (*newInput)->totalElapsedSeconds = totalSeconds;
    (*newInput)->frameCounter = frameCounter;

    // Copy over key & mouse state
    (*newInput)->hasKeyMouseInput = true;
    (*newInput)->keyMouse.mouseX = (*oldInput)->keyMouse.mouseX;
    (*newInput)->keyMouse.mouseY = (*oldInput)->keyMouse.mouseY;
    (*newInput)->keyMouse.mouseZ = (*oldInput)->keyMouse.mouseZ;
    (*newInput)->keyMouse.mouseRawXDelta = 0;
    (*newInput)->keyMouse.mouseRawYDelta = 0;
    (*newInput)->keyMouse.mouseRawZDelta = 0;

    for( int i = 0; i < ARRAYCOUNT((*newInput)->keyMouse.mouseButtons); ++i )
        (*newInput)->keyMouse.mouseButtons[i] = (*oldInput)->keyMouse.mouseButtons[i];
    for( int i = 0; i < ARRAYCOUNT((*newInput)->keyMouse.keysDown); ++i )
        (*newInput)->keyMouse.keysDown[i] = (*oldInput)->keyMouse.keysDown[i];
}


internal void
Win32GetInputFilePath( const Win32State& platformState, int slotIndex, bool isInputStream, char* destination )
{
    char buffer[PLATFORM_PATH_MAX];
    sprintf_s( buffer, ARRAYCOUNT(buffer), "%s%d%s%s", "gamestate", slotIndex, isInputStream ? "_input" : "", ".in" );

    Win32JoinPaths( platformState.binFolderPath, buffer, destination, false );
}

internal Win32ReplayBuffer*
Win32GetReplayBuffer( Win32State* platformState, int index )
{
    ASSERT( index < ARRAYCOUNT(platformState->replayBuffers) );
    Win32ReplayBuffer *replayBuffer = &platformState->replayBuffers[index];

    return replayBuffer;
}

internal void
Win32BeginInputRecording( Win32State *platformState, int inputRecordingIndex )
{
    // Since we use an index value of 0 as an off flag, valid indices start at 1
    ASSERT( inputRecordingIndex > 0 && inputRecordingIndex < MAX_REPLAY_BUFFERS + 1 );
    Win32ReplayBuffer *replayBuffer = Win32GetReplayBuffer( platformState, inputRecordingIndex - 1 );

    if( replayBuffer->memoryBlock )
    {
        platformState->inputRecordingIndex = inputRecordingIndex;

        char filename[PLATFORM_PATH_MAX];
        Win32GetInputFilePath( *platformState, inputRecordingIndex, true, filename );
        platformState->recordingHandle = CreateFile( filename,
                                                     GENERIC_WRITE, 0, 0,
                                                     CREATE_ALWAYS, 0, 0 );

        CopyMemory( replayBuffer->memoryBlock, platformState->gameMemoryBlock, platformState->gameMemorySize );
        // TODO Write state to disk asynchronously

        LOG( "Started input recording at slot %d", inputRecordingIndex );
    }
}

internal void
Win32EndInputRecording( Win32State *platformState )
{
    CloseHandle( platformState->recordingHandle );
    platformState->inputRecordingIndex = 0;
}

internal void
Win32RecordInput( const Win32State& platformState, GameInput *newInput )
{
    DWORD bytesWritten;
    WriteFile( platformState.recordingHandle, newInput, sizeof(*newInput),
               &bytesWritten, 0 );
}

internal void
Win32BeginInputPlayback( Win32State *platformState, int inputPlaybackIndex )
{
    // Since we use an index value of 0 as an off flag, valid indices start at 1
    ASSERT( inputPlaybackIndex > 0 && inputPlaybackIndex < MAX_REPLAY_BUFFERS + 1 );
    Win32ReplayBuffer *replayBuffer = Win32GetReplayBuffer( platformState, inputPlaybackIndex - 1 );

    if( replayBuffer->memoryBlock )
    {
        platformState->inputPlaybackIndex = inputPlaybackIndex;

        char filename[PLATFORM_PATH_MAX];
        Win32GetInputFilePath( *platformState, inputPlaybackIndex, true, filename );
        platformState->playbackHandle = CreateFile( filename,
                                                    GENERIC_READ, 0, 0,
                                                    OPEN_EXISTING, 0, 0 );

        CopyMemory( platformState->gameMemoryBlock, replayBuffer->memoryBlock, platformState->gameMemorySize );
        
        LOG( "Started input playback at slot %d", inputPlaybackIndex );
    }
}

internal void
Win32EndInputPlayback( Win32State *platformState )
{
    CloseHandle( platformState->playbackHandle );
    platformState->inputPlaybackIndex = 0;
    
    LOG( "Input playback finished" );
}

internal void
Win32PlayBackInput( Win32State *platformState, GameInput *newInput )
{
    DWORD bytesRead;
    if( ReadFile( platformState->playbackHandle, newInput, sizeof(*newInput),
                                  &bytesRead, 0 ) == FALSE || bytesRead == 0 )
    {
        i32 playingIndex = platformState->inputPlaybackIndex;
        Win32EndInputPlayback( platformState );
        Win32BeginInputPlayback( platformState, playingIndex );
        ReadFile( platformState->playbackHandle, newInput, sizeof(*newInput),
                  &bytesRead, 0 );
    }
}


inline LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter( &result );
    return result;
}

inline f32
Win32GetSecondsElapsed( LARGE_INTEGER start, LARGE_INTEGER end )
{
    f32 result = (f32)(end.QuadPart - start.QuadPart) / (f32)globalPerfCounterFrequency;
    return result;
}

DEBUG_PLATFORM_CURRENT_TIME_MILLIS(Win32CurrentTimeMillis)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter( &counter );
    f64 result = (f64)counter.QuadPart / globalPerfCounterFrequency * 1000;
    return result;
}


internal void
Win32ToggleFullscreen( HWND window )
{
    persistent WINDOWPLACEMENT windowPos = {sizeof(windowPos)};

    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    LONG style = GetWindowLong( window, GWL_STYLE );
    if( style & WS_OVERLAPPEDWINDOW )
    {
        MONITORINFO monitorInfo = {sizeof(monitorInfo)};
        if( GetWindowPlacement( window, &windowPos )
            && GetMonitorInfo( MonitorFromWindow( window, MONITOR_DEFAULTTOPRIMARY ), &monitorInfo ) )
        {
            SetWindowLong( window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW );
            SetWindowPos( window, HWND_TOP,
                          monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                          monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                          monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                          SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
        }
    }
    else
    {
        SetWindowLong( window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW );
        SetWindowPlacement( window, &windowPos );
        SetWindowPos( window, 0, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                      SWP_NOOWNERZORDER | SWP_FRAMECHANGED );
    }
}

internal void
Win32HideWindow( HWND window )
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    wp.flags = 0;
    wp.showCmd = SW_HIDE;
    wp.ptMinPosition = {0};
    wp.ptMaxPosition = {0};
    wp.rcNormalPosition = {0};

    SetWindowPlacement( window, &wp );
}

#if !RELEASE
void
Win32ToggleGlobalDebugging( GameMemory *gameMemory, HWND window )
{
    gameMemory->DEBUGglobalDebugging = !gameMemory->DEBUGglobalDebugging;
#if 0
    SetCursor( gameMemory->DEBUGglobalDebugging ? DEBUGglobalCursor : 0 );

    LONG_PTR curStyle = GetWindowLongPtr( window,
                                          GWL_EXSTYLE );
    LONG_PTR newStyle = gameMemory->DEBUGglobalDebugging
        ? (curStyle | WS_EX_LAYERED)
        : (curStyle & ~WS_EX_LAYERED);

    SetWindowLongPtr( window, GWL_EXSTYLE,
                      newStyle );
    SetWindowPos( window, gameMemory->DEBUGglobalDebugging
                  ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                  SWP_NOMOVE | SWP_NOSIZE );
#endif
}
#endif

internal WPARAM
Win32MapLeftRightKeys( WPARAM wParam, LPARAM lParam )
{
    WPARAM new_vk = wParam;
    UINT scancode = UINT( (lParam & 0x00ff0000) >> 16 );
    int extended  = (lParam & 0x01000000) != 0;

    switch (wParam)
    {
    case VK_SHIFT:
        new_vk = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
        break;
    case VK_CONTROL:
        new_vk = WPARAM( extended ? VK_RCONTROL : VK_LCONTROL );
        break;
    case VK_MENU:
        new_vk = WPARAM( extended ? VK_RMENU : VK_LMENU );
        break;
    default:
        // not a key we map from generic to left/right specialized
        //  just return it.
        new_vk = wParam;
        break;    
    }

    return new_vk;
}

internal void
Win32ProcessPendingMessages( Win32State *platformState, GameMemory *gameMemory,
                             GameInput *input, GameControllerInput *keyMouseController )
{
    MSG message;
    auto& imGuiIO = ImGui::GetIO();

    while( PeekMessage( &message, 0, 0, 0, PM_REMOVE ) )
    {
        switch( message.message )
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                // Allow WM_CHAR messages to be sent
                TranslateMessage( &message );

                bool wasDown = ((message.lParam & (1 << 30)) != 0);
                bool isDown =  ((message.lParam & (1 << 31)) == 0);

                bool extended  = (message.lParam & 0x01000000) != 0;
                u32 scanCode = u32( ((message.lParam >> 16) & 0x7f) | (extended ? 0x80 : 0) );

                // TODO Use scancodes too for all special keys!?
                // @Test
                message.wParam = Win32MapLeftRightKeys( message.wParam, message.lParam );
                u32 vkCode = U32( message.wParam );
                if( vkCode < 256 )
                    imGuiIO.KeysDown[vkCode] = isDown ? 1 : 0;

                bool lShiftDown = (GetAsyncKeyState( VK_LSHIFT ) & 0x8000) != 0;
                bool rShiftDown = (GetAsyncKeyState( VK_RSHIFT ) & 0x8000) != 0;
                bool rControlDown = (GetAsyncKeyState( VK_RCONTROL ) & 0x8000) != 0;
                bool lAltDown = (GetAsyncKeyState( VK_LMENU ) & 0x8000) != 0;
                bool rAltDown = (GetAsyncKeyState( VK_RMENU ) & 0x8000) != 0;
                // HACK L-CTRL & R-ALT are triggered together!
                bool lControlDown = (GetAsyncKeyState( VK_LCONTROL ) & 0x8000) != 0
                    && !rAltDown;

                bool shiftKeyDown = lShiftDown || rShiftDown;
                bool ctrlKeyDown = lControlDown || rControlDown;
                bool altKeyDown = lAltDown || rAltDown;

                // Set controller button states (for both up/down transitions)
                // (don't ever send keys to game if ImGui is handling them)
                if( isDown != wasDown && !imGuiIO.WantCaptureKeyboard )
                {
                    // Translate key code into the game's platform agnostic codes
                    if( scanCode < ARRAYCOUNT(Win32NativeToHID) )
                    {
                        u32 hidCode = Win32NativeToHID[scanCode];
                        if( hidCode < ARRAYCOUNT(input->keyMouse.keysDown) )
                            input->keyMouse.keysDown[hidCode] = isDown;
                    }

                    // Emulate controller from keys
                    if( vkCode == 'W' )
                    {
                        Win32SetButtonState( &keyMouseController->dUp, isDown );
                        // Simulate fake stick
                        keyMouseController->leftStick.avgY += isDown ? 1 : -1;
                    }
                    else if( vkCode == 'A' )
                    {
                        Win32SetButtonState( &keyMouseController->dLeft, isDown );
                        keyMouseController->leftStick.avgX += isDown ? -1 : 1;
                    }
                    else if( vkCode == 'S' )
                    {
                        Win32SetButtonState( &keyMouseController->dDown, isDown );
                        keyMouseController->leftStick.avgY += isDown ? -1 : 1;
                    }
                    else if( vkCode == 'D' )
                    {
                        Win32SetButtonState( &keyMouseController->dRight, isDown );
                        keyMouseController->leftStick.avgX += isDown ? 1 : -1;
                    }
                    else if( vkCode == VK_LSHIFT )
                    {
                        Win32SetButtonState( &keyMouseController->leftThumb, isDown );
                    }
                    else if( vkCode == VK_RSHIFT )
                    {
                        Win32SetButtonState( &keyMouseController->rightThumb, isDown );
                    }
                    else if( vkCode == VK_LCONTROL )
                    {
                        Win32SetButtonState( &keyMouseController->leftShoulder, isDown );
                    }
                    else if( vkCode == VK_SPACE )
                    {
                        Win32SetButtonState( &keyMouseController->rightShoulder, isDown );
                    }
                    else if( vkCode == VK_UP )
                    {
                    }
                    else if( vkCode == VK_DOWN )
                    {
                    }
                    else if( vkCode == VK_LEFT )
                    {
                    }
                    else if( vkCode == VK_RIGHT )
                    {
                    }
                    else if( vkCode == VK_RETURN )
                    {
                        Win32SetButtonState( &keyMouseController->aButton, isDown );
                    }
                }

                // Exit & debug/edit modes
                if( isDown && !imGuiIO.WantCaptureKeyboard )
                {
                    if( vkCode == VK_F4 && altKeyDown )
                    {
                        globalRunning = false;
                    }
#if !RELEASE
                    if( vkCode == VK_RETURN && altKeyDown )
                    {
                        Win32ToggleFullscreen( platformState->mainWindow );
                    }
                    else if( vkCode == VK_ESCAPE )
                    {
                        if( platformState->inputPlaybackIndex )
                        {
                            Win32EndInputPlayback( platformState );
                            Win32ResetController( keyMouseController );
                        }
                        else if( gameMemory->DEBUGglobalDebugging )
                        {
                            Win32ToggleGlobalDebugging( gameMemory, platformState->mainWindow );
                        }
                        else if( gameMemory->DEBUGglobalEditing )
                        {
                            gameMemory->DEBUGglobalEditing = false;
                        }
                        else
                        {
                            globalRunning = false;
                        }
                    }

                    // @Test The "key above TAB"
                    else if( vkCode == VK_OEM_3 || vkCode == VK_OEM_5 )
                    {
                        if( !gameMemory->DEBUGglobalDebugging )
                            Win32ToggleGlobalDebugging( gameMemory, platformState->mainWindow );
                    }

                    else if( vkCode == VK_F1 )
                    {
                        if( !gameMemory->DEBUGglobalDebugging && !gameMemory->DEBUGglobalEditing )
                        {
                            if( platformState->inputPlaybackIndex == 0 )
                            {
                                if( platformState->inputRecordingIndex == 0 )
                                {
                                    Win32BeginInputRecording( platformState, 1 );
                                }
                                else if( platformState->inputRecordingIndex == 1 )
                                {
                                    Win32EndInputRecording( platformState );
                                    Win32BeginInputPlayback( platformState, 1 );
                                }
                            }
                        }
                    }

                    // FIXME F12 Crashes!
                    else if( vkCode == VK_F11 )
                    {
                        gameMemory->DEBUGglobalEditing = !gameMemory->DEBUGglobalEditing;
                    }
#endif
                }
            } break;

            case WM_CHAR:
            {
                u32 ch = (u32)message.wParam;
                if( ch > 0 && ch < 0x10000 )
                    imGuiIO.AddInputCharacter( (ImWchar)ch );
            } break;

// Copied from windowsx.h
#define GET_X_LPARAM(lp)    ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)    ((int)(short)HIWORD(lp))

            case WM_MOUSEMOVE:
            {
                // Can be negative on multiple monitors
                input->keyMouse.mouseX = GET_X_LPARAM( message.lParam );
                input->keyMouse.mouseY = GET_Y_LPARAM( message.lParam );
                
                imGuiIO.MousePos.x = (float)input->keyMouse.mouseX;
                imGuiIO.MousePos.y = (float)input->keyMouse.mouseY;
            } break;

            case WM_MOUSEWHEEL:
            {
                input->keyMouse.mouseZ = GET_WHEEL_DELTA_WPARAM( message.wParam );

                imGuiIO.MouseWheel = input->keyMouse.mouseZ > 0 ? +1.0f : -1.0f;
            } break;

            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_XBUTTONDOWN:
            {
                // These are only received when over the client area
                SetCapture( platformState->mainWindow );
            } break;

            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            case WM_XBUTTONUP:
            {
                ReleaseCapture();
            } break;

            case WM_INPUT: 
            {
                UINT dwSize = 256;
                static BYTE lpb[256];
                UINT res = GetRawInputData( (HRAWINPUT)message.lParam, RID_INPUT, 
                                 lpb, &dwSize, sizeof(RAWINPUTHEADER) );
                ASSERT( res != (UINT)-1 );
                RAWINPUT* raw = (RAWINPUT*)lpb;

                if (raw->header.dwType == RIM_TYPEMOUSE) 
                {
                    if( raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE )
                    {
                        i32 xMotionRelative = raw->data.mouse.lLastX;
                        i32 yMotionRelative = raw->data.mouse.lLastY;
                        input->keyMouse.mouseRawXDelta += xMotionRelative;
                        input->keyMouse.mouseRawYDelta += yMotionRelative;
                        keyMouseController->rightStick.avgX += xMotionRelative;
                        keyMouseController->rightStick.avgY += yMotionRelative;
                    }

                    bool bUp, bDown;
                    USHORT buttonFlags = raw->data.mouse.usButtonFlags;

                    bUp = (buttonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->keyMouse.mouseButtons[MouseButtonLeft], bDown );
                        imGuiIO.MouseDown[0] = input->keyMouse.mouseButtons[MouseButtonLeft].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->keyMouse.mouseButtons[MouseButtonMiddle], bDown );
                        imGuiIO.MouseDown[1] = input->keyMouse.mouseButtons[MouseButtonMiddle].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->keyMouse.mouseButtons[MouseButtonRight], bDown );
                        imGuiIO.MouseDown[2] = input->keyMouse.mouseButtons[MouseButtonRight].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_BUTTON_4_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_BUTTON_4_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->keyMouse.mouseButtons[MouseButton4], bDown  );
                        imGuiIO.MouseDown[3] = input->keyMouse.mouseButtons[MouseButton4].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_BUTTON_5_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_BUTTON_5_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->keyMouse.mouseButtons[MouseButton5], bDown  );
                        imGuiIO.MouseDown[4] = input->keyMouse.mouseButtons[MouseButton5].endedDown;
                    }

                    if( buttonFlags & RI_MOUSE_WHEEL )
                    {
                        f32 zMotionRelative = (f32)(i16)raw->data.mouse.usButtonData / (f32)WHEEL_DELTA;
                        input->keyMouse.mouseRawZDelta += zMotionRelative;
                        //keyMouseController->rightStick.avgZ += zMotionRelative;
                    }
                } 
            } break;

            default:
            {
                TranslateMessage( &message );
                DispatchMessage( &message );
            } break;
        }
    }
}

LRESULT CALLBACK
Win32WindowProc( HWND hwnd, UINT  uMsg, WPARAM wParam, LPARAM lParam )
{
    LRESULT result = 0;
    
    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
        {
            if( wParam == TRUE )
            {
                SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 255, LWA_ALPHA );
            }
            else
            {
                SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 128, LWA_ALPHA );
            }
        } break;

        //case WM_SIZE:
        //{
        //} break;

        //case WM_SETCURSOR:
        //{
            //SetCursor( DEBUGglobalCursor );
        //} break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            ASSERT( !"Received keyboard input in WindowProc!" );
        } break;

        case WM_CLOSE:
        {
            globalRunning = false;
        } break;

        case WM_QUIT:
        {
            globalRunning = false;
        } break;

        case WM_DESTROY:
        {
            PostQuitMessage( 0 );
        } break;

        default:
        {
            result = DefWindowProc( hwnd, uMsg, wParam, lParam );
        } break;
    }
    
    return result;
}


internal void
Win32ResolvePaths( Win32State *state )
{
    DWORD pathLen = GetCurrentDirectory( ARRAYCOUNT(state->currentDirectory), state->currentDirectory );
    LOG( state->currentDirectory );

    pathLen = GetModuleFileName( 0, state->exeFilePath, ARRAYCOUNT(state->exeFilePath) );
    Win32GetParentPath( state->exeFilePath, state->binFolderPath );

    {
        bool result = false;
        char buffer[] = "robotrider.dll";

#if !RELEASE
        // This is just to support the dist mechanism
        {
            char* sourceDLLName = "robotrider.debug.dll";
            Win32JoinPaths( state->binFolderPath, sourceDLLName, state->sourceDLLPath, false );
            if( PathFileExists( state->sourceDLLPath ) )
            {
                result = true;
            }
        }
#endif

        char *sourceDLLName = buffer;
        if( !result )
        {
            Win32JoinPaths( state->binFolderPath, sourceDLLName, state->sourceDLLPath, false );
            if( PathFileExists( state->sourceDLLPath ) )
            {
                result = true;
            }
        }

        if( result )
        {
            TrimFileExtension( sourceDLLName );
            char tempDLLPath[PLATFORM_PATH_MAX];
            sprintf_s( state->tempDLLPath, ARRAYCOUNT(state->tempDLLPath), "%s\\%s.temp.dll",
                       state->binFolderPath, sourceDLLName );
        }

        ASSERT( result );
        if( !result )
            LOG( ".FATAL: Could not find game DLL!" );

        Win32JoinPaths( state->binFolderPath, "dll.lock", state->lockFilePath, false );
    }

    {
        bool result = false;

        const char* supportedDataPaths[] =
        {
            "data",
            "../data",
        };

        for( int i = 0; i < ARRAYCOUNT( supportedDataPaths ); ++i )
        {
            Win32JoinPaths( state->binFolderPath, supportedDataPaths[i], state->dataFolderPath, true );
            if( PathFileExists( state->dataFolderPath ) )
            {
                result = true;
                break;
            }
        }

        ASSERT( result );
        if( !result )
            LOG( ".FATAL: Could not data root folder!" );
    }
}


internal bool
Win32InitOpenGL( HDC dc, const RenderCommands& commands, int frameVSyncSkipCount )
{
    LOG( ".Initializing OpenGL..." );

    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    int formatIndex = ChoosePixelFormat( dc, &pfd );
    if( !formatIndex )
    {
        LOG( ".ERROR: ChoosePixelFormat failed!" );
        return false;
    }

    if( !SetPixelFormat( dc, formatIndex, &pfd ) )
    {
        LOG( ".ERROR: SetPixelFormat failed!" );
        return false;
    }

    HGLRC legacyContext = wglCreateContext( dc );
    if( !legacyContext )
    {
        GLenum error = glGetError();
        LOG( ".ERROR: wglCreateContext failed with code %u", error );
        return false;
    }

    if( !wglMakeCurrent( dc, legacyContext ) )
    {
        GLenum error = glGetError();
        LOG( ".ERROR: wglMakeCurrent failed with code %u", error );
        return false;
    }

    int flags = 0;
    //flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#if !RELEASE
    flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

    const int contextAttributes[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, flags,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        //WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB
        = (PFNWGLCREATECONTEXTATTRIBSARBPROC)(void*)wglGetProcAddress( "wglCreateContextAttribsARB" );
    if( !wglCreateContextAttribsARB )
    {
        LOG( ".ERROR: Failed querying entry point for wglCreateContextAttribsARB!" );
        return false;
    }

    HGLRC renderingContext = wglCreateContextAttribsARB( dc, 0, contextAttributes );
    if( !renderingContext )
    {
        GLenum error = glGetError();
        LOG( ".ERROR: Couldn't create rendering context! Error code is: %u", error );
        return false;
    }

    // Destroy dummy context
    BOOL res;
    res = wglMakeCurrent( dc, NULL );
    res = wglDeleteContext( legacyContext );

    if( !wglMakeCurrent( dc, renderingContext ) )
    {
        GLenum error = glGetError();
        LOG( ".ERROR: wglMakeCurrent failed with code %u", error );
        return false;
    }

    // Success
    // Setup pointers to cross platform extension functions
#define BINDGLPROC( name, type ) name = (type)(void*)wglGetProcAddress( #name )
    BINDGLPROC( glGetStringi, PFNGLGETSTRINGIPROC );
    BINDGLPROC( glGenBuffers, PFNGLGENBUFFERSPROC );
    BINDGLPROC( glBindBuffer, PFNGLBINDBUFFERPROC );
    BINDGLPROC( glBufferData, PFNGLBUFFERDATAPROC );
    BINDGLPROC( glBufferSubData, PFNGLBUFFERSUBDATAPROC );
    BINDGLPROC( glCreateShader, PFNGLCREATESHADERPROC );
    BINDGLPROC( glShaderSource, PFNGLSHADERSOURCEPROC );
    BINDGLPROC( glCompileShader, PFNGLCOMPILESHADERPROC );
    BINDGLPROC( glGetShaderiv, PFNGLGETSHADERIVPROC );
    BINDGLPROC( glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC );
    BINDGLPROC( glCreateProgram, PFNGLCREATEPROGRAMPROC );
    BINDGLPROC( glAttachShader, PFNGLATTACHSHADERPROC );
    BINDGLPROC( glLinkProgram, PFNGLLINKPROGRAMPROC );
    BINDGLPROC( glGetProgramiv, PFNGLGETPROGRAMIVPROC );
    BINDGLPROC( glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC );
    BINDGLPROC( glDeleteShader, PFNGLDELETESHADERPROC );
    BINDGLPROC( glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC );
    BINDGLPROC( glBindVertexArray, PFNGLBINDVERTEXARRAYPROC );
    BINDGLPROC( glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC );
    BINDGLPROC( glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC );
    BINDGLPROC( glUseProgram, PFNGLUSEPROGRAMPROC );
    BINDGLPROC( glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC );
    BINDGLPROC( glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC );
    BINDGLPROC( glDebugMessageCallbackARB, PFNGLDEBUGMESSAGECALLBACKARBPROC );
    BINDGLPROC( glGetAttribLocation, PFNGLGETATTRIBLOCATIONPROC );
    BINDGLPROC( glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC );
    BINDGLPROC( glActiveTexture, PFNGLACTIVETEXTUREPROC );
    BINDGLPROC( glBlendEquation, PFNGLBLENDEQUATIONPROC );
    BINDGLPROC( glUniform1i, PFNGLUNIFORM1IPROC );
    BINDGLPROC( glBindAttribLocation, PFNGLBINDATTRIBLOCATIONPROC );
    BINDGLPROC( glVertexAttribIPointer, PFNGLVERTEXATTRIBIPOINTERPROC );
    BINDGLPROC( glGenerateMipmap, PFNGLGENERATEMIPMAPPROC );
    BINDGLPROC( glGenQueries, PFNGLGENQUERIESPROC );
    BINDGLPROC( glBeginQuery, PFNGLBEGINQUERYPROC );
    BINDGLPROC( glEndQuery, PFNGLENDQUERYPROC );
    BINDGLPROC( glGetQueryObjectuiv, PFNGLGETQUERYOBJECTUIVPROC );
    BINDGLPROC( glVertexAttribDivisor, PFNGLVERTEXATTRIBDIVISORARBPROC );
    BINDGLPROC( glDrawArraysInstanced, PFNGLDRAWARRAYSINSTANCEDPROC );
    BINDGLPROC( glDrawElementsInstanced, PFNGLDRAWELEMENTSINSTANCEDPROC );
    BINDGLPROC( glDrawElementsBaseVertex, PFNGLDRAWELEMENTSBASEVERTEXPROC );
    BINDGLPROC( glUniform3fv, PFNGLUNIFORM3FVPROC );
    BINDGLPROC( glUniform1ui, PFNGLUNIFORM1UIPROC );
#undef BINDGLPROC


    OpenGLInfo info = OpenGLInit( globalOpenGLState, true );


    // VSync
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)(void*)wglGetProcAddress( "wglSwapIntervalEXT" );

    ASSERT( wglSwapIntervalEXT );
    if( globalVSyncEnabled )
        wglSwapIntervalEXT( frameVSyncSkipCount );
    else
        wglSwapIntervalEXT( 0 );

    return true;
}

internal bool
Win32DoNextQueuedJob( PlatformJobQueue* queue, int workerThreadIndex )
{
    bool workAvailable = true;

    u32 observedValue = queue->nextJobToRead;
    u32 desiredValue = (observedValue + 1) % ARRAYCOUNT(queue->jobs);

    if( observedValue != queue->nextJobToWrite )
    {
        u32 index = AtomicCompareExchange( &queue->nextJobToRead, desiredValue, observedValue );
        if( index == observedValue )
        {
            PlatformJobQueueJob job = queue->jobs[index];
            job.callback( job.userData, workerThreadIndex );
            
            AtomicAdd( &queue->completionCount, 1 );
        }
    }
    else
        workAvailable = false;

    return workAvailable;
}

internal DWORD WINAPI
Win32WorkerThreadProc( LPVOID lpParam )
{
    Win32WorkerThreadContext* context = (Win32WorkerThreadContext*)lpParam;
    PlatformJobQueue* queue = context->queue;

    while( true )
    {
        if( !Win32DoNextQueuedJob( queue, context->threadIndex ) )
        {
            WaitForSingleObjectEx( queue->semaphore, INFINITE, FALSE );
        }
    }

    return 0;
}

#define MAIN_THREAD_WORKER_INDEX 0

internal void
Win32InitJobQueue( PlatformJobQueue* queue,
                   Win32WorkerThreadContext* threadContexts, int threadCount )
{
    *queue = {0};
    queue->semaphore = CreateSemaphoreEx( 0, 0, threadCount,
                                          0, 0, SEMAPHORE_ALL_ACCESS );

    for( int i = 0; i < threadCount; ++i )
    {
        // FIXME Separate platform thread infos from the queues!
        // TODO Consider creating an arena per thread (specially for hi-prio threads!)
        threadContexts[i] = { i, queue };

        // Worker thread index 0 is reserved for the main thread!
        if( i > MAIN_THREAD_WORKER_INDEX )
        {
            DWORD threadId;
            HANDLE handle = CreateThread( 0, MEGABYTES(1),
                                          Win32WorkerThreadProc,
                                          &threadContexts[i], 0, &threadId );
            CloseHandle( handle );
        }
    }
}

internal
PLATFORM_ADD_NEW_JOB(Win32AddNewJob)
{
    // NOTE Single producer
    ASSERT( (queue->nextJobToWrite + 1) % ARRAYCOUNT(queue->jobs) != queue->nextJobToRead );

    PlatformJobQueueJob& job = queue->jobs[queue->nextJobToWrite];
    job = { callback, userData };
    ++queue->completionTarget;

    MEMORY_WRITE_BARRIER;

    queue->nextJobToWrite = (queue->nextJobToWrite + 1) % ARRAYCOUNT(queue->jobs);
    ReleaseSemaphore( queue->semaphore, 1, 0 );
}

internal
PLATFORM_COMPLETE_ALL_JOBS(Win32CompleteAllJobs)
{
    //
    // FIXME Assert that this is only called from the main thread!
    //
    //
    while( queue->completionCount < queue->completionTarget )
    {
        Win32DoNextQueuedJob( queue, MAIN_THREAD_WORKER_INDEX );
    }

    queue->completionTarget = 0;
    queue->completionCount = 0;
}


internal
PLATFORM_ALLOCATE_OR_UPDATE_TEXTURE(Win32AllocateTexture)
{
    void* result = nullptr;

    switch( globalPlatformState.renderer )
    {
        case Renderer::OpenGL:
            result = OpenGLAllocateTexture( data, width, height, filtered, optionalHandle );
            break;

        INVALID_DEFAULT_CASE
    }

    return result;
}

internal
PLATFORM_DEALLOCATE_TEXTURE(Win32DeallocateTexture)
{
    switch( globalPlatformState.renderer )
    {
        case Renderer::OpenGL:
            OpenGLDeallocateTexture( handle );
            break;

        INVALID_DEFAULT_CASE
    }
}


int 
main( int argC, char **argV )
{
    SYSTEM_INFO systemInfo;
    GetSystemInfo( &systemInfo );

    // Init global platform
    globalPlatform.Log = Win32Log;
    globalPlatform.DEBUGReadEntireFile = DEBUGWin32ReadEntireFile;
    globalPlatform.DEBUGFreeFileMemory = DEBUGWin32FreeFileMemory;
    globalPlatform.DEBUGWriteEntireFile = DEBUGWin32WriteEntireFile;
    globalPlatform.DEBUGListAllAssets = Win32ListAllAssets;
    globalPlatform.DEBUGJoinPaths = Win32JoinPaths;
    globalPlatform.DEBUGGetParentPath = Win32GetParentPath;
    globalPlatform.DEBUGCurrentTimeMillis = Win32CurrentTimeMillis;
    globalPlatform.AddNewJob = Win32AddNewJob;
    globalPlatform.CompleteAllJobs = Win32CompleteAllJobs;
    globalPlatform.AllocateOrUpdateTexture = Win32AllocateTexture;
    globalPlatform.DeallocateTexture = Win32DeallocateTexture;

    // FIXME Should be dynamic, but can't be bothered!
    Win32WorkerThreadContext threadContexts[32];
    int coreCount = int( systemInfo.dwNumberOfProcessors );
    ASSERT( coreCount <= ARRAYCOUNT(threadContexts) );

    Win32InitJobQueue( &globalPlatformState.hiPriorityQueue, threadContexts, coreCount );
    globalPlatform.hiPriorityQueue = &globalPlatformState.hiPriorityQueue;
    globalPlatform.coreThreadsCount = coreCount;

    globalPlatformState.renderer = Renderer::OpenGL;


    Win32ResolvePaths( &globalPlatformState );

    LOG( "Initializing Win32 platform with game DLL at: %s", globalPlatformState.sourceDLLPath );
    GameMemory gameMemory = {};
    gameMemory.permanentStorageSize = GIGABYTES(2);
    gameMemory.transientStorageSize = GIGABYTES(2);
#if !RELEASE
    gameMemory.debugStorageSize = MEGABYTES(64);
#endif
    gameMemory.platformAPI = &globalPlatform;

    // Init subsystems
    Win32InitXInput();
    // TODO Test what a safe value for buffer size/latency is with several audio cards
    // (stress test by artificially lowering the framerate)
    Win32AudioOutput audioOutput = Win32InitWASAPI( 48000, AUDIO_BITDEPTH, AUDIO_CHANNELS, AUDIO_LATENCY_SAMPLES );

    u32 runningFrameCounter = 0;
    f32 totalElapsedSeconds = 0;
    LARGE_INTEGER perfCounterFreqMeasure;
    QueryPerformanceFrequency( &perfCounterFreqMeasure );
    globalPerfCounterFrequency = perfCounterFreqMeasure.QuadPart;

#if 0
    // Set Windows scheduler granularity so that our frame wait sleep is more granular
    bool sleepIsGranular = (timeBeginPeriod( 1 ) == TIMERR_NOERROR);
    if( !sleepIsGranular )
    {
        LOG( ".Warning: We don't have a granular scheduler!" );
    }
#endif

    // Register window class and create game window
    WNDCLASS windowClass = {};
    windowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    windowClass.lpfnWndProc = Win32WindowProc;
    windowClass.hInstance = GetModuleHandle( NULL );
    windowClass.lpszClassName = "RobotRiderWindowClass";
    windowClass.hCursor = LoadCursor( NULL, IDC_ARROW );

    LOG( ".Creating window..." );

    if( RegisterClass( &windowClass ) )
    {
        HWND window = CreateWindowEx( 0, 
                                      windowClass.lpszClassName,
                                      "RobotRider",
                                      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0,
                                      0,
                                      windowClass.hInstance,
                                      0 );
                                            
        if( window )
        {
            HDC deviceContext = GetDC( window );

            // Get monitor refresh rate
            globalMonitorRefreshHz = VIDEO_TARGET_FRAMERATE;
            int refreshRate = GetDeviceCaps( deviceContext, VREFRESH );
            if( refreshRate > 1 )
            {
                globalMonitorRefreshHz = refreshRate;
                LOG( ".Monitor refresh rate: %d Hz", globalMonitorRefreshHz );
            }
            else
            {
                LOG( ".WARNING: Failed to query monitor refresh rate. Using %d Hz", VIDEO_TARGET_FRAMERATE );
            }
            int frameVSyncSkipCount = I32( Round( (f32)globalMonitorRefreshHz / VIDEO_TARGET_FRAMERATE ) );
            int videoTargetFramerateHz = globalMonitorRefreshHz / frameVSyncSkipCount;

            // Determine system latency
            REFERENCE_TIME latency;
            globalAudioClient->GetStreamLatency( &latency );
            audioOutput.systemLatencyFrames = U16( Ceil( (u64)latency * audioOutput.samplingRate / 10000000.0 ) );
            u32 audioLatencyFrames = audioOutput.samplingRate / videoTargetFramerateHz;

#if !RELEASE
            DEBUGglobalCursor = LoadCursor( 0, IDC_CROSS );
            ShowWindow( window, SW_MAXIMIZE ); //SW_SHOWNORMAL );
#else
            Win32ToggleFullscreen( window );
#endif

            globalPlatformState.mainWindow = window;
            Win32RegisterRawMouseInput( window );

            // TODO Decide a proper size for this
            sz renderBufferSize = MEGABYTES( 4 );
            u8 *renderBuffer = (u8 *)VirtualAlloc( 0, renderBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            sz vertexBufferMaxCount = MEGABYTES( 32 ); // Million vertices
            TexturedVertex *vertexBuffer = (TexturedVertex *)VirtualAlloc( 0, vertexBufferMaxCount * sizeof(TexturedVertex),
                                                                           MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
            sz indexBufferMaxCount = vertexBufferMaxCount * 8;
            i32 *indexBuffer = (i32 *)VirtualAlloc( 0, indexBufferMaxCount * sizeof(i32), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            sz instanceBufferSize = MEGABYTES( 256 );
            u8 *instanceBuffer = (u8 *)VirtualAlloc( 0, instanceBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            RenderCommands renderCommands = InitRenderCommands( renderBuffer, I32( renderBufferSize ),
                                                                vertexBuffer, I32( vertexBufferMaxCount ),
                                                                indexBuffer, I32( indexBufferMaxCount ),
                                                                instanceBuffer, I32( instanceBufferSize ) );

            if( Win32InitOpenGL( deviceContext, renderCommands, frameVSyncSkipCount ) )
            {
                LPVOID baseAddress = 0;

                // TODO Make simple growable arenas for these, that _reserve_ say 8Gb each, but only commit in sizeable blocks as needed
                // Allocate game memory pools
                u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
#if !RELEASE
                baseAddress = (LPVOID)GIGABYTES(2048);
                totalSize += gameMemory.debugStorageSize;
#endif
                // TODO Use MEM_LARGE_PAGES and call AdjustTokenPrivileges when not in XP
                globalPlatformState.gameMemoryBlock = VirtualAlloc( baseAddress, totalSize,
                                                                    MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
                globalPlatformState.gameMemorySize = totalSize;

                gameMemory.permanentStorage = globalPlatformState.gameMemoryBlock;
                gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize;
#if !RELEASE
                gameMemory.debugStorage = (u8*)gameMemory.transientStorage + gameMemory.transientStorageSize;
#endif

                i16 *soundSamples = (i16 *)VirtualAlloc( 0, audioOutput.bufferSizeFrames * audioOutput.bytesPerFrame,
                                                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

#if !RELEASE
                for( int replayIndex = 0; replayIndex < ARRAYCOUNT(globalPlatformState.replayBuffers); ++replayIndex )
                {
                    Win32ReplayBuffer *replayBuffer = &globalPlatformState.replayBuffers[replayIndex];

                    // Since we use an index value of 0 as an off flag, slot filenames start at 1
                    Win32GetInputFilePath( globalPlatformState, replayIndex + 1, false, replayBuffer->filename );
                    replayBuffer->fileHandle = CreateFile( replayBuffer->filename,
                                                           GENERIC_READ|GENERIC_WRITE, 0, 0,
                                                           CREATE_ALWAYS, 0, 0 );
                    DWORD ignored;
                    DeviceIoControl( globalPlatformState.recordingHandle, FSCTL_SET_SPARSE, 0, 0, 0, 0, &ignored, 0 );

                    replayBuffer->memoryMap = CreateFileMapping( replayBuffer->fileHandle,
                                                                 0, PAGE_READWRITE,
                                                                 (globalPlatformState.gameMemorySize >> 32),
                                                                 globalPlatformState.gameMemorySize & 0xFFFFFFFF,
                                                                 0 );

                    replayBuffer->memoryBlock = MapViewOfFile( replayBuffer->memoryMap,
                                                               FILE_MAP_ALL_ACCESS,
                                                               0, 0, globalPlatformState.gameMemorySize );
                    if( !replayBuffer->memoryBlock )
                    {
                        // TODO Diagnostic
                    }
                }
#endif

                gameMemory.imGuiContext = Win32InitImGui( window );

                if( gameMemory.permanentStorage && renderCommands.isValid && soundSamples )
                {
                    LOG( ".Allocated game memory with base address: %p", baseAddress );

                    globalPlatformState.gameCode
                        = Win32LoadGameCode( globalPlatformState.sourceDLLPath, globalPlatformState.tempDLLPath, &gameMemory );
                    ASSERT( globalPlatformState.gameCode.isValid );

                    Win32AssetUpdateListener assetListeners[] =
                    {
                        { SHADERS_RELATIVE_PATH, "glsl", OpenGLHotswapShader },
                    };
                    globalPlatformState.assetListeners = assetListeners;
                    globalPlatformState.assetListenerCount = ARRAYCOUNT(assetListeners);
                    Win32SetupAssetUpdateListeners( &globalPlatformState );

                    HRESULT audioStarted = globalAudioClient->Start();
                    ASSERT( audioStarted == S_OK );

                    GameInput input[2] = {};
                    GameInput *newInput = &input[0];
                    GameInput *oldInput = &input[1];

                    LARGE_INTEGER firstClock = Win32GetWallClock();
                    LARGE_INTEGER lastClock = firstClock;
                    u64 lastCycleCounter = Rdtsc();

                    f32 targetElapsedPerFrameSecs = 1.0f / videoTargetFramerateHz;
                    // Assume our target for the first frame
                    f32 frameElapsedSeconds = targetElapsedPerFrameSecs;

                    globalRunning = true;

                    // Main loop
                    while( globalRunning )
                    {
#if !RELEASE
                        DebugFrameInfo debugFrameInfo = {};
                        
                        // Prevent huge skips in physics etc. while debugging
                        if( frameElapsedSeconds > 1.f )
                        {
                            frameElapsedSeconds = targetElapsedPerFrameSecs;
                            totalElapsedSeconds += frameElapsedSeconds;
                        }
                        else
#endif
                            totalElapsedSeconds = Win32GetSecondsElapsed( firstClock, lastClock );

                        Win32PrepareInputData( &oldInput, &newInput, frameElapsedSeconds, totalElapsedSeconds, runningFrameCounter );
                        if( runningFrameCounter == 0 )
                            newInput->gameCodeReloaded = true;

#if !RELEASE
                        // Check for game code updates
                        FILETIME dllWriteTime = Win32GetLastWriteTime( globalPlatformState.sourceDLLPath );
                        if( CompareFileTime( &dllWriteTime, &globalPlatformState.gameCode.lastDLLWriteTime ) != 0 )
                        {
                            Win32CompleteAllJobs( globalPlatform.hiPriorityQueue );

                            if( !globalPlatformState.gameCodeReloading )
                                LOG( "Detected updated game DLL. Reloading.." );
                            Win32UnloadGameCode( &globalPlatformState.gameCode );

                            WIN32_FILE_ATTRIBUTE_DATA data;
                            // Check for a dll.lock file created by build.py, meaning the compiler hasn't finished executing yet
                            // TODO Do something that doesnt require an external scheme.
                            // Maybe just loop trying to read until we stop receiving a sharing violation
                            if( !GetFileAttributesEx( globalPlatformState.lockFilePath, GetFileExInfoStandard, &data ) )
                            {
                                globalPlatformState.gameCode = Win32LoadGameCode( globalPlatformState.sourceDLLPath,
                                                                                  globalPlatformState.tempDLLPath,
                                                                                  &gameMemory );
                                globalPlatformState.gameCodeReloading = false;
                                newInput->gameCodeReloaded = true;
                                LOG( "Game code reloaded" );
                            }
                            else
                            {
                                if( !globalPlatformState.gameCodeReloading )
                                {
                                    LOG( "Waiting for DLL lock.." );
                                    globalPlatformState.gameCodeReloading = true;
                                }
                            }
                        }

                        // Check for asset updates
                        Win32CheckAssetUpdates( &globalPlatformState );
#endif

                        Win32WindowDimension windowDim = Win32GetWindowDimension( window );
                        renderCommands.width = (u16)windowDim.width;
                        renderCommands.height = (u16)windowDim.height;

                        // Process input
                        GameControllerInput *newKeyMouseController = Win32ResetKeyMouseController( oldInput, newInput );
                        Win32ProcessPendingMessages( &globalPlatformState, &gameMemory, newInput, newKeyMouseController );
                        Win32ProcessXInputControllers( oldInput, newInput, newKeyMouseController );

#if !RELEASE
                        debugFrameInfo.inputProcessedCycles = Rdtsc() - lastCycleCounter;

                        if( gameMemory.DEBUGglobalDebugging )
                        {
                            // Discard all input to the key&mouse controller
                            Win32ResetKeyMouseController( oldInput, newInput );
                        }

                        if( globalPlatformState.inputRecordingIndex )
                        {
                            Win32RecordInput( globalPlatformState, newInput );
                        }
                        if( globalPlatformState.inputPlaybackIndex )
                        {
                            Win32PlayBackInput( &globalPlatformState, newInput );
                        }
#endif

                        // Setup remaining stuff for the ImGui frame
                        ImGuiIO &io = ImGui::GetIO();
                        io.DeltaTime = frameElapsedSeconds;
                        io.DisplaySize.x = (f32)windowDim.width;
                        io.DisplaySize.y = (f32)windowDim.height;
                        io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                        io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                        io.KeySuper = false;
                        ImGui::NewFrame();

                        int audioFramesToWrite = 0;
                        u32 audioPaddingFrames;
                        if( SUCCEEDED(globalAudioClient->GetCurrentPadding( &audioPaddingFrames )) )
                        {
                            // TODO Try priming the buffer with something like half a frame's worth of silence

                            audioFramesToWrite = audioOutput.bufferSizeFrames - I32( audioPaddingFrames );
                            ASSERT( audioFramesToWrite >= 0 );
                        }

                        // Prepare audio & video buffers
                        GameAudioBuffer audioBuffer = {};
                        audioBuffer.samplesPerSecond = audioOutput.samplingRate;
                        audioBuffer.channelCount = AUDIO_CHANNELS;
                        audioBuffer.frameCount = audioFramesToWrite;
                        audioBuffer.samples = soundSamples;

                        ResetRenderCommands( &renderCommands );


                        // Ask the game to render one frame
                        globalPlatformState.gameCode.UpdateAndRender( &gameMemory, newInput, &renderCommands, &audioBuffer );

#if !RELEASE
                        debugFrameInfo.gameUpdatedCycles = Rdtsc() - lastCycleCounter;
#endif

                        // Blit audio buffer to output
                        Win32BlitAudioBuffer( &audioBuffer, audioFramesToWrite, &audioOutput );

#if !RELEASE
                        debugFrameInfo.audioUpdatedCycles = Rdtsc() - lastCycleCounter;
#endif

#if 0
                        i64 endCycleCounter = Rdtsc();
                        u64 cyclesElapsed = endCycleCounter - lastCycleCounter;
                        u32 kCyclesElapsed = (u32)(cyclesElapsed / 1000);

                        // Artificially increase wait time from 0 to 20ms.
                        int r = rand() % 20;
                        targetElapsedPerFrameSecs = (1.0f / videoTargetFramerateHz) + ((f32)r / 1000.0f);
#endif
#if 0
                        LARGE_INTEGER endClock = Win32GetWallClock();
                        f32 frameElapsedSecs = Win32GetSecondsElapsed( lastClock, endClock );

                        // Wait till the target frame time
                        f32 elapsedSecs = frameElapsedSecs;
                        if( elapsedSecs < targetElapsedPerFrameSecs )
                        {
                            if( sleepIsGranular )
                            {
                                DWORD sleepMs = (DWORD)(1000.0f * (targetElapsedPerFrameSecs - frameElapsedSecs));
                                if( sleepMs > 0 )
                                {
                                    Sleep( sleepMs );
                                }
                            }
                            while( elapsedSecs < targetElapsedPerFrameSecs )
                            {
                                elapsedSecs = Win32GetSecondsElapsed( lastClock, Win32GetWallClock() );
                            }
                        }
                        else
                        {
                            // TODO Log missed frame rate
                        }
#endif

                        // Blit video to output
                        Win32DisplayInWindow( globalPlatformState, renderCommands, deviceContext,
                                              windowDim.width, windowDim.height, &gameMemory );

                        LARGE_INTEGER endClock = Win32GetWallClock();
                        frameElapsedSeconds = Win32GetSecondsElapsed( lastClock, endClock );
                        lastClock = endClock;

                        u64 endCycleCounter = Rdtsc();
                        u64 totalFrameCycles = endCycleCounter - lastCycleCounter;
                        lastCycleCounter = endCycleCounter;

#if !RELEASE
                        debugFrameInfo.totalFrameCycles = totalFrameCycles;
                        debugFrameInfo.totalFrameSeconds = frameElapsedSeconds;

                        if( globalPlatformState.gameCode.DebugFrameEnd )
                            globalPlatformState.gameCode.DebugFrameEnd( debugFrameInfo, &gameMemory );
#endif

                        ++runningFrameCounter;

#if 0
                        {
                            f32 fps = 1.0f / frameElapsedSeconds;
                            LOG( "ms: %.02f - FPS: %.02f (%d Kcycles) - audio padding: %d\n",
                                 1000.0f * frameElapsedSeconds, fps, kCyclesElapsed, audioPaddingFrames );
                        }
#endif

                    }
                }
                else
                {
                    LOG( ".ERROR: Memory allocation failed!" );
                }
            }
            else
            {
                LOG( ".ERROR: OpenGL initialization failed!" );
            }
        }
        else
        {
            LOG( ".ERROR: Failed creating window!" );
        }
    }
    else
    {
        LOG( ".ERROR: Couldn't register window class!" );
    }

    // FIXME This is causing a hang. Investigate!
    //Win32CompleteAllJobs( globalPlatform.hiPriorityQueue );

    LOG( "\n\nFPS: %.1f imm. / %.1f avg.",
         ImGui::GetIO().Framerate, (f32)runningFrameCounter / totalElapsedSeconds );

#if !RELEASE
    DebugState* debugState = (DebugState*)gameMemory.debugStorage;

    LOG( ":::Counter totals:" );
    for( int i = 0; i < debugState->counterLogsCount; ++i )
    {
        DebugCounterLog &log = debugState->counterLogs[i];
        if( log.totalHits > 0 )
        {
            LOG( "%s\t%llu tc  %u h  %llu tc/h",
                 log.name,
                 log.totalCycles,
                 log.totalHits,
                 log.totalCycles / log.totalHits );
        }
    }
#endif

    return 0;
}

