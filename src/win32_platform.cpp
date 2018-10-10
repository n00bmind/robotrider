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

#include <windows.h>

#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
#include "imgui/imgui_widgets.cpp"

#include <gl/gl.h>
#include "glext.h"
#include "wglext.h"
#define GL_DEBUG_CALLBACK(name) \
    void WINAPI name( GLenum source, GLenum type, GLuint id, GLenum severity, \
                      GLsizei length, const GLchar *message, const void *userParam )
#include "opengl_renderer.h"
#include "opengl_renderer.cpp"

#include <xinput.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <shlwapi.h>
#include <stdio.h>


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
internal u32 globalMonitorRefreshHz;
internal IAudioClient* globalAudioClient;
internal IAudioRenderClient* globalAudioRenderClient;
internal i64 globalPerfCounterFrequency;
#if !RELEASE
internal HCURSOR DEBUGglobalCursor;
#endif


// Global switches
internal bool globalVSyncEnabled = false;



internal sz
Win32JoinPaths( const char *pathBase, const char *relativePath, char *destination, bool addTrailingSlash = true )
{
    char buffer[MAX_PATH];

    sz len = Min( strlen( pathBase ) + 1, MAX_PATH );
    strncpy( buffer, pathBase, len );
    len = MAX_PATH - len;
    strncat( buffer, relativePath, len );
    
    if( addTrailingSlash )
    {
        len = strlen( buffer );
        if( buffer[len-1] != '/' )
        {
            if( len < MAX_PATH )
                buffer[len++] = '/';
            if( len < MAX_PATH )
                buffer[len++] = 0;
        }
    }

    GetFullPathName( buffer, MAX_PATH, destination, nullptr );
    return strlen( destination );
}

internal void
Win32GetFullPathToFile( const char* rootPath, const char* filename, char* destination )
{
    Win32JoinPaths( rootPath, filename, destination, false );
}

internal bool
RemoveFilenameFromPath( char *path )
{
    bool result = false;

    char *onePastLastSlash = path;
    for( char *scanChar = path; *scanChar; ++scanChar )
    {
        if( *scanChar == '\\' )
        {
            onePastLastSlash = scanChar + 1;
        }
    }
    if( onePastLastSlash != path )
    {
        *onePastLastSlash = 0;
        result = true;
    }

    return result;
}

internal char *
ExtractFileExtension( char *filename, char *destination, u32 destinationMaxLen )
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
    char* extensionPos = ExtractFileExtension( filename, nullptr, 0 );
    *extensionPos = 0;
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

    char absolutePath[MAX_PATH];
    if( PathIsRelative( filename ) )
    {
        // If path is relative, use executable location to complete it
        Win32GetFullPathToFile( globalPlatformState.assetDataPath, filename, absolutePath );
        filename = absolutePath;
    }

    HANDLE fileHandle = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    if( fileHandle != INVALID_HANDLE_VALUE )
    {
        LARGE_INTEGER fileSize;
        if( GetFileSizeEx( fileHandle, &fileSize ) )
        {
            u32 fileSize32 = SafeTruncToU32( fileSize.QuadPart );
            result.contents = VirtualAlloc( 0, fileSize32 + 1, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            if( result.contents )
            {
                DWORD bytesRead;
                if( ReadFile( fileHandle, result.contents, fileSize32, &bytesRead, 0 )
                    && (fileSize32 == bytesRead) )
                {
                    // Null-terminate to help when handling text files
                    *((u8 *)result.contents + fileSize32) = '\0';
                    result.contentSize = fileSize32 + 1;
                }
                else
                {
                    LOG( "ERROR: ReadFile failed" );
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
            LOG( "ERROR: Failed querying file size" );
        }

        CloseHandle( fileHandle );
    }
    else
    {
        LOG( "ERROR: Failed opening file for reading" );
    }

    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGWin32WriteEntireFile)
{
    bool result = false;

    char absolutePath[MAX_PATH];
    if( PathIsRelative( filename ) )
    {
        // If path is relative, use executable location to complete it
        Win32GetFullPathToFile( globalPlatformState.assetDataPath, filename, absolutePath );
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

    CopyFile( sourceDLLPath, tempDLLPath, FALSE );
    result.gameCodeDLL = LoadLibrary( tempDLLPath );
    if( result.gameCodeDLL )
    {
        result.SetupAfterReload = (GameSetupAfterReloadFunc *)GetProcAddress( result.gameCodeDLL, "GameSetupAfterReload" );
        result.UpdateAndRender = (GameUpdateAndRenderFunc *)GetProcAddress( result.gameCodeDLL, "GameUpdateAndRender" );
        result.LogCallback = (GameLogCallbackFunc *)GetProcAddress( result.gameCodeDLL, "GameLogCallback" );
        result.DebugFrameEnd = (DebugGameFrameEndFunc *)GetProcAddress( result.gameCodeDLL, "DebugGameFrameEnd" );

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
Win32SetupAssetUpdateListener( Win32State *platformState )
{
    char absolutePath[MAX_PATH];
    // FIXME This part is OpenGL specific. Formalize how we get this for different renderers
    Win32JoinPaths( platformState->assetDataPath, SHADERS_RELATIVE_PATH, absolutePath );

    platformState->shadersDirHandle
        = CreateFile( absolutePath, FILE_LIST_DIRECTORY, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL );

    if( platformState->shadersDirHandle == INVALID_HANDLE_VALUE )
    {
        LOG( "ERROR :: Could not open directory for reading (%s)", absolutePath );
        return;
    }

    platformState->shadersOverlapped = {0};
    platformState->shadersOverlapped.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    BOOL read = ReadDirectoryChangesW( platformState->shadersDirHandle,
                                       platformState->shadersNotifyBuffer,
                                       ARRAYCOUNT(platformState->shadersNotifyBuffer),
                                       TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE,
                                       NULL, &platformState->shadersOverlapped, NULL );
    ASSERT( read );
}

internal void
Win32CheckAssetUpdates( Win32State *platformState )
{
    DWORD bytesReturned;
    // Return immediately if no info ready
    BOOL result = GetOverlappedResult( platformState->shadersDirHandle,
                                       &platformState->shadersOverlapped,
                                       &bytesReturned, FALSE );

    if( result )
    {
        FILE_NOTIFY_INFORMATION *notifyInfo
            = (FILE_NOTIFY_INFORMATION *)platformState->shadersNotifyBuffer;
        if( notifyInfo->Action == FILE_ACTION_MODIFIED )
        {
            char filename[MAX_PATH];
            WideCharToMultiByte( CP_ACP, WC_COMPOSITECHECK,
                                 notifyInfo->FileName, notifyInfo->FileNameLength,
                                 filename, MAX_PATH, NULL, NULL );
            filename[notifyInfo->FileNameLength/2] = 0;

            // Check and strip extension
            char extension[16];
            ExtractFileExtension( filename, extension, ARRAYCOUNT(extension) );
            
            if( strcmp( extension, "glsl" ) == 0 )
            {
                char assetPath[MAX_PATH];
                strcpy( assetPath, SHADERS_RELATIVE_PATH );
                strcat( assetPath, filename );
                
                LOG( "Shader file '%s' was modified. Reloading..", filename );
                DEBUGReadFileResult read = globalPlatform.DEBUGReadEntireFile( assetPath );
                ASSERT( read.contents );
                OpenGLHotswapShader( filename, (const char *)read.contents );
                globalPlatform.DEBUGFreeFileMemory( read.contents );
            }
        }

        ASSERT( notifyInfo->NextEntryOffset == 0 );
        //LOG( "WARNING :: More data available in update listener" );

        // Restart monitorization
        BOOL read = ReadDirectoryChangesW( platformState->shadersDirHandle,
                                           platformState->shadersNotifyBuffer,
                                           ARRAYCOUNT(platformState->shadersNotifyBuffer),
                                           TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE,
                                           NULL, &platformState->shadersOverlapped, NULL );
        ASSERT( read );
    }
    else
    {
        ASSERT( GetLastError() == ERROR_IO_INCOMPLETE );
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
        XInputGetState = (XInputGetStateFunc *)GetProcAddress( XInputLibrary, "XInputGetState" );
        if( !XInputGetState ) { XInputGetState = XInputGetStateStub; }

        XInputSetState = (XInputSetStateFunc *)GetProcAddress( XInputLibrary, "XInputSetState" );
        if( !XInputSetState ) { XInputSetState = XInputSetStateStub; }
    }
    else
    {
        LOG( ".ERROR: Couldn't load xinput1_3.dll!" );
    }
}


// WASAPI functions
internal Win32AudioOutput
Win32InitWASAPI( u32 samplingRate, u16 bitDepth, u16 channelCount, u32 bufferSizeFrames )
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
    REFERENCE_TIME bufferDuration = 10000000ULL * bufferSizeFrames / samplingRate;     // buffer size in hundreds of nanoseconds (1 sec.)
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

    ASSERT( bufferSizeFrames == audioFramesCount );

    audioOutput.samplingRate = waveFormat.Format.nSamplesPerSec;
    audioOutput.bytesPerFrame = AUDIO_BITDEPTH * AUDIO_CHANNELS / 8;
    audioOutput.bufferSizeFrames = bufferSizeFrames;
    audioOutput.runningFrameCount = 0;

    return audioOutput;
}

internal void
Win32BlitAudioBuffer( GameAudioBuffer *sourceBuffer, u32 framesToWrite, Win32AudioOutput *audioOutput )
{
    BYTE *audioData;
    HRESULT hr = globalAudioRenderClient->GetBuffer( framesToWrite, &audioData );
    if( hr == S_OK )
    {
        i16* sourceSample = sourceBuffer->samples;
        i16* destSample = (i16*)audioData;
        for( u32 frameIndex = 0; frameIndex < framesToWrite; ++frameIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++audioOutput->runningFrameCount;
        }

        globalAudioRenderClient->ReleaseBuffer( framesToWrite, 0 );
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
    buffer->memory = VirtualAlloc( 0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE );
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

    for( u32 buttonIndex = 0;
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

internal r32
Win32ProcessXInputStickValue( SHORT value, SHORT deadZoneThreshold )
{
    // TODO Cube the values as explained in the XInput help
    // to give more sensitivity to the movement
    r32 result = 0;
    // TODO Does hardware have a round deadzone?
    if( value < -deadZoneThreshold )
    {
        result = (r32)(value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if( value > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
    {
        result = (r32)(value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

internal void
Win32ProcessXInputControllers( GameInput* oldInput, GameInput* newInput )
{
    // TODO Don't call GetState on disconnected controllers cause that has a big impact
    // TODO Should we poll this more frequently?
    // First controller is the keyboard
    // TODO Mouse?
    u32 maxControllerCount = XUSER_MAX_COUNT;
    if( maxControllerCount > ARRAYCOUNT( newInput->_controllers ) - 1 )
    {
        maxControllerCount = ARRAYCOUNT( newInput->_controllers ) - 1;
    }

    for( u32 index = 0; index < maxControllerCount; ++index )
    {
        DWORD controllerIndex = index + 1;
        GameControllerInput *oldController = GetController( oldInput, controllerIndex );
        GameControllerInput *newController = GetController( newInput, controllerIndex );

        XINPUT_STATE controllerState;
        // FIXME This call apparently stalls for a few hundred thousand cycles when the controller is not present
        if( XInputGetState( controllerIndex, &controllerState ) == ERROR_SUCCESS )
        {
            // Plugged in
            newController->isConnected = true;
            newController->isAnalog = oldController->isAnalog;
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

            if( newController->leftStick.avgX != 0.0f || newController->leftStick.avgY != 0.0f ||
                newController->rightStick.avgX != 0.0f || newController->rightStick.avgY != 0.0f )
            {
                newController->isAnalog = true;
            }

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
            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
            {
                newController->leftStick.avgY = 1.0f;
                newController->isAnalog = false;
            }
            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
            {
                newController->leftStick.avgY = -1.0f;
                newController->isAnalog = false;
            }
            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
            {
                newController->leftStick.avgX = -1.0f;
                newController->isAnalog = false;
            }
            if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
            {
                newController->leftStick.avgX = 1.0f;
                newController->isAnalog = false;
            }

            r32 threshold = 0.5f;
            Win32ProcessXInputDigitalButton( (newController->leftStick.avgX < -threshold) ? 1 : 0,
                                            &oldController->dLeft, 1,
                                            &newController->dLeft );
            Win32ProcessXInputDigitalButton( (newController->leftStick.avgX > threshold) ? 1 : 0,
                                            &oldController->dRight, 1,
                                            &newController->dRight );
            Win32ProcessXInputDigitalButton( (newController->leftStick.avgY < -threshold) ? 1 : 0,
                                            &oldController->dDown, 1,
                                            &newController->dDown );
            Win32ProcessXInputDigitalButton( (newController->leftStick.avgY > threshold) ? 1 : 0,
                                            &oldController->dUp, 1,
                                            &newController->dUp );
#endif
        }
        else
        {
            // Controller not available
            newController->isConnected = false;
        }
    }

}

internal void
Win32PrepareInputData( GameInput *&oldInput, GameInput *&newInput,
                       float elapsedSeconds, float totalSeconds, u32 frameCounter )
{
    GameInput *temp = newInput;
    newInput = oldInput;
    oldInput = temp;

    newInput->executableReloaded = false;
    newInput->mouseX = oldInput->mouseX;
    newInput->mouseY = oldInput->mouseY;
    newInput->mouseZ = oldInput->mouseZ;
    for( u32 i = 0; i < ARRAYCOUNT(newInput->mouseButtons); ++i )
        newInput->mouseButtons[i] = oldInput->mouseButtons[i];
    newInput->frameElapsedSeconds = elapsedSeconds;
    newInput->totalElapsedSeconds = totalSeconds;
    newInput->frameCounter = frameCounter;
}


internal void
Win32GetInputFilePath( const Win32State& platformState, u32 slotIndex, bool isInputStream,
                       char* dest, u32 destCount )
{
    sprintf_s( dest, destCount, "%s%s%d%s%s", platformState.exeFilePath,
               "gamestate", slotIndex, isInputStream ? "_input" : "", ".in" );
}

internal Win32ReplayBuffer*
Win32GetReplayBuffer( Win32State* platformState, u32 index )
{
    ASSERT( index < ARRAYCOUNT(platformState->replayBuffers) );
    Win32ReplayBuffer *replayBuffer = &platformState->replayBuffers[index];

    return replayBuffer;
}

internal void
Win32BeginInputRecording( Win32State *platformState, u32 inputRecordingIndex )
{
    // Since we use an index value of 0 as an off flag, valid indices start at 1
    ASSERT( inputRecordingIndex > 0 && inputRecordingIndex < MAX_REPLAY_BUFFERS + 1 );
    Win32ReplayBuffer *replayBuffer = Win32GetReplayBuffer( platformState, inputRecordingIndex - 1 );

    if( replayBuffer->memoryBlock )
    {
        platformState->inputRecordingIndex = inputRecordingIndex;

        char filename[MAX_PATH];
        Win32GetInputFilePath( *platformState, inputRecordingIndex, true,
                               filename, ARRAYCOUNT(filename) );
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
Win32BeginInputPlayback( Win32State *platformState, u32 inputPlaybackIndex )
{
    // Since we use an index value of 0 as an off flag, valid indices start at 1
    ASSERT( inputPlaybackIndex > 0 && inputPlaybackIndex < MAX_REPLAY_BUFFERS + 1 );
    Win32ReplayBuffer *replayBuffer = Win32GetReplayBuffer( platformState, inputPlaybackIndex - 1 );

    if( replayBuffer->memoryBlock )
    {
        platformState->inputPlaybackIndex = inputPlaybackIndex;

        char filename[MAX_PATH];
        Win32GetInputFilePath( *platformState, inputPlaybackIndex, true,
                               filename, ARRAYCOUNT(filename) );
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
        u32 playingIndex = platformState->inputPlaybackIndex;
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

inline r32
Win32GetSecondsElapsed( LARGE_INTEGER start, LARGE_INTEGER end )
{
    r32 result = (r32)(end.QuadPart - start.QuadPart) / (r32)globalPerfCounterFrequency;
    return result;
}


internal void
Win32ToggleFullscreen( HWND window )
{
    persistent WINDOWPLACEMENT windowPos = {sizeof(windowPos)};

    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD style = GetWindowLong( window, GWL_STYLE );
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
}
#endif

internal void
Win32ProcessPendingMessages( Win32State *platformState, GameMemory *gameMemory,
                             GameInput *input, GameControllerInput *keyMouseController )
{
    MSG message;
    bool altKeyDown, ctrlKeyDown, shiftKeyDown;
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

                u32 vkCode = SafeTruncToU32( message.wParam );
                if( vkCode < 256 )
                    imGuiIO.KeysDown[vkCode] = isDown ? 1 : 0;

                altKeyDown = (GetKeyState( VK_MENU ) & 0x8000) != 0;
                ctrlKeyDown = (GetKeyState( VK_CONTROL ) & 0x8000) != 0;
                shiftKeyDown = (GetKeyState( VK_SHIFT ) & 0x8000) != 0;

                // Set controller button states (only for up/down transitions)
                // (don't ever send keys to game if ImGui is handling them)
                if( isDown != wasDown && !imGuiIO.WantCaptureKeyboard )
                {
                    if( vkCode == 'W' )
                    {
                        Win32SetButtonState( &keyMouseController->dUp, isDown );
                        // Simulate fake stick
                        keyMouseController->leftStick.avgY += isDown ? 0.5f : -0.5f;
                    }
                    else if( vkCode == 'A' )
                    {
                        Win32SetButtonState( &keyMouseController->dLeft, isDown );
                        keyMouseController->leftStick.avgX += isDown ? -0.5f : 0.5f;
                    }
                    else if( vkCode == 'S' )
                    {
                        Win32SetButtonState( &keyMouseController->dDown, isDown );
                        keyMouseController->leftStick.avgY += isDown ? -0.5f : 0.5f;
                    }
                    else if( vkCode == 'D' )
                    {
                        Win32SetButtonState( &keyMouseController->dRight, isDown );
                        keyMouseController->leftStick.avgX += isDown ? 0.5f : -0.5f;
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

                // Respond to keyboard input
                if( isDown )
                {
#if RELEASE
                    if( vkCode == VK_ESCAPE ||
                        (vkCode == VK_F4 && altKeyDown) )
                    {
                        globalRunning = false;
                    }
#else
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

                    // FIXME This only works on a spanish keyboard it seems
                    else if( vkCode == VK_OEM_5 )
                    {
                        if( ctrlKeyDown )
                            gameMemory->DEBUGglobalEditing = true;
                        else if( !gameMemory->DEBUGglobalDebugging && !gameMemory->DEBUGglobalEditing )
                            Win32ToggleGlobalDebugging( gameMemory, platformState->mainWindow );
                    }
                    else if( vkCode == VK_F1 )
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
#endif
                }
            } break;

            case WM_CHAR:
            {
                u32 ch = (u32)message.wParam;
                if( ch > 0 && ch < 0x10000 )
                    imGuiIO.AddInputCharacter( (ImWchar)ch );
            } break;

#define GET_SIGNED_LO(dw) ((int)(short)LOWORD(dw))
#define GET_SIGNED_HI(dw) ((int)(short)HIWORD(dw))

            case WM_MOUSEMOVE:
            {
                input->mouseX = GET_SIGNED_LO( message.lParam );
                input->mouseY = GET_SIGNED_HI( message.lParam );
                
                imGuiIO.MousePos.x = (float)input->mouseX;
                imGuiIO.MousePos.y = (float)input->mouseY;
            } break;

            case WM_MOUSEWHEEL:
            {
                POINT pt;
                pt.x = GET_SIGNED_LO( message.lParam );
                pt.y = GET_SIGNED_HI( message.lParam );
                ScreenToClient( platformState->mainWindow, &pt );
                input->mouseX = pt.x;
                input->mouseY = pt.y;
                input->mouseZ = GET_WHEEL_DELTA_WPARAM( message.wParam );

                imGuiIO.MouseWheel = input->mouseZ > 0 ? +1.0f : -1.0f;
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
                        int xPosRelative = raw->data.mouse.lLastX;
                        int yPosRelative = raw->data.mouse.lLastY;
                        keyMouseController->rightStick.avgX += xPosRelative;
                        keyMouseController->rightStick.avgY += yPosRelative;
                    }

                    bool bUp, bDown;
                    USHORT buttonFlags = raw->data.mouse.usButtonFlags;

                    bUp = (buttonFlags & RI_MOUSE_LEFT_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[0], bDown );
                        imGuiIO.MouseDown[0] = !!input->mouseButtons[0].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[1], bDown );
                        imGuiIO.MouseDown[1] = !!input->mouseButtons[1].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[2], bDown );
                        imGuiIO.MouseDown[2] = !!input->mouseButtons[2].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_BUTTON_4_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_BUTTON_4_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[3], bDown  );
                        imGuiIO.MouseDown[3] = !!input->mouseButtons[3].endedDown;
                    }
                    bUp = (buttonFlags & RI_MOUSE_BUTTON_5_UP) != 0;
                    bDown = (buttonFlags & RI_MOUSE_BUTTON_5_DOWN) != 0;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[4], bDown  );
                        imGuiIO.MouseDown[4] = !!input->mouseButtons[4].endedDown;
                    }

                    if( buttonFlags & RI_MOUSE_WHEEL )
                    {
                        int zPosRelative = raw->data.mouse.usButtonData;
                        //keyMouseController->rightStick.avgZ += zPosRelative;
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
    RemoveFilenameFromPath( state->exeFilePath );

    {
        bool result = false;
        char buffer[] = "robotrider.dll";

#if !RELEASE
        // This is just to support the dist mechanism
        {
            char* sourceDLLName = "robotrider.debug.dll";
            sprintf_s( state->sourceDLLPath, ARRAYCOUNT(state->sourceDLLPath), "%s%s", globalPlatformState.exeFilePath, sourceDLLName );
            if( PathFileExists( state->sourceDLLPath ) )
            {
                result = true;
            }
        }
#endif

        char *sourceDLLName = buffer;
        if( !result )
        {
            sprintf_s( state->sourceDLLPath, ARRAYCOUNT(state->sourceDLLPath), "%s%s", globalPlatformState.exeFilePath, sourceDLLName );
            if( PathFileExists( state->sourceDLLPath ) )
            {
                result = true;
            }
        }

        if( result )
        {
            TrimFileExtension( sourceDLLName );
            char tempDLLPath[MAX_PATH];
            sprintf_s( state->tempDLLPath, ARRAYCOUNT(state->tempDLLPath), "%s%s.temp.dll", globalPlatformState.exeFilePath, sourceDLLName );
        }

        ASSERT( result );
        if( !result )
            LOG( ".FATAL: Could not find game DLL!" );
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
            Win32JoinPaths( state->exeFilePath, supportedDataPaths[i], state->assetDataPath );
            if( PathFileExists( state->assetDataPath ) )
            {
                result = true;
                break;
            }
        }

        ASSERT( result );
        if( !result )
            LOG( ".FATAL: Could not find game DLL!" );
    }
}


internal bool
Win32InitOpenGL( HDC dc, const RenderCommands& commands, u32 frameVSyncSkipCount )
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
        int error = glGetError();
        LOG( ".ERROR: wglCreateContext failed with code %d", error );
        return false;
    }

    if( !wglMakeCurrent( dc, legacyContext ) )
    {
        int error = glGetError();
        LOG( ".ERROR: wglMakeCurrent failed with code %d", error );
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
        = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress( "wglCreateContextAttribsARB" );
    if( !wglCreateContextAttribsARB )
    {
        LOG( ".ERROR: Failed querying entry point for wglCreateContextAttribsARB!" );
        return false;
    }

    HGLRC renderingContext = wglCreateContextAttribsARB( dc, 0, contextAttributes );
    if( !renderingContext )
    {
        int error = glGetError();
        LOG( ".ERROR: Couldn't create rendering context! Error code is: %d", error );
        return false;
    }

    // Destroy dummy context
    BOOL res;
    res = wglMakeCurrent( dc, NULL );
    res = wglDeleteContext( legacyContext );

    if( !wglMakeCurrent( dc, renderingContext ) )
    {
        int error = glGetError();
        LOG( ".ERROR: wglMakeCurrent failed with code %d", error );
        return false;
    }

    // Success
    // Setup pointers to cross platform extension functions
#define BINDGLPROC( name, type ) name = (type)wglGetProcAddress( #name )
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
#undef BINDGLPROC


    OpenGLInfo info = OpenGLInit( globalOpenGLState, true );


    // VSync
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress( "wglSwapIntervalEXT" );

    ASSERT( wglSwapIntervalEXT );
    if( globalVSyncEnabled )
        wglSwapIntervalEXT( frameVSyncSkipCount );
    else
        wglSwapIntervalEXT( 0 );

    return true;
}

internal bool
Win32DoNextQueuedJob( PlatformJobQueue* queue, u32 workerThreadIndex )
{
    bool didJob = false;

    u32 observedValue = queue->nextJobToRead;
    u32 desiredValue = (observedValue + 1) % ARRAYCOUNT(queue->jobs);

    if( observedValue != queue->nextJobToWrite )
    {
        u32 index = AtomicCompareExchangeU32( &queue->nextJobToRead,
                                              desiredValue,
                                              observedValue );
        if( index == observedValue )
        {
            PlatformJobQueueJob job = queue->jobs[index];
            job.callback( job.userData, workerThreadIndex );
            
            AtomicAddU32( &queue->completionCount, 1 );
            didJob = true;
        }
    }
    return didJob;
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
                   Win32WorkerThreadContext* threadContexts, u32 threadCount )
{
    *queue = {0};
    queue->semaphore = CreateSemaphoreEx( 0, 0, threadCount,
                                          0, 0, SEMAPHORE_ALL_ACCESS );

    for( u32 i = 0; i < threadCount; ++i )
    {
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
PLATFORM_ALLOCATE_TEXTURE(Win32AllocateTexture)
{
    void* result = nullptr;

    switch( globalPlatformState.renderer )
    {
        case Renderer::OpenGL:
            result = OpenGLAllocateTexture( data, width, height, filtered );
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
    globalPlatform.AddNewJob = Win32AddNewJob;
    globalPlatform.CompleteAllJobs = Win32CompleteAllJobs;
    globalPlatform.AllocateTexture = Win32AllocateTexture;
    globalPlatform.DeallocateTexture = Win32DeallocateTexture;

    // FIXME Should be dynamic, but can't be bothered!
    Win32WorkerThreadContext threadContexts[32];
    u32 workerThreadsCount = systemInfo.dwNumberOfProcessors;
    ASSERT( workerThreadsCount <= ARRAYCOUNT(threadContexts) );

    Win32InitJobQueue( &globalPlatformState.hiPriorityQueue, threadContexts, workerThreadsCount );
    globalPlatform.hiPriorityQueue = &globalPlatformState.hiPriorityQueue;
    globalPlatform.workerThreadsCount = workerThreadsCount;

    globalPlatformState.renderer = Renderer::OpenGL;


    Win32ResolvePaths( &globalPlatformState );

    LOG( "Initializing Win32 platform with game DLL at: %s", globalPlatformState.sourceDLLPath );
    GameMemory gameMemory = {};
    gameMemory.permanentStorageSize = GIGABYTES(2);
    gameMemory.transientStorageSize = GIGABYTES(1);
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
    r32 totalElapsedSeconds = 0;
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
            u32 frameVSyncSkipCount = Round( (r32)globalMonitorRefreshHz / VIDEO_TARGET_FRAMERATE );
            u32 videoTargetFramerateHz = globalMonitorRefreshHz / frameVSyncSkipCount;

            // Determine system latency
            REFERENCE_TIME latency;
            globalAudioClient->GetStreamLatency( &latency );
            audioOutput.systemLatencyFrames = (u16)Ceil( (u64)latency * audioOutput.samplingRate / 10000000.0 );
            u32 audioLatencyFrames = audioOutput.samplingRate / videoTargetFramerateHz;

#if !RELEASE
            DEBUGglobalCursor = LoadCursor( 0, IDC_CROSS );
            ShowWindow( window, SW_MAXIMIZE );
#else
            Win32ToggleFullscreen( window );
#endif

            globalPlatformState.mainWindow = window;
            Win32RegisterRawMouseInput( window );

            // TODO Decide a proper size for this
            u32 renderBufferSize = MEGABYTES( 4 );
            u8 *renderBuffer = (u8 *)VirtualAlloc( 0, renderBufferSize,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
            u32 vertexBufferMaxCount = 8*1024*1024;
            TexturedVertex *vertexBuffer = (TexturedVertex *)VirtualAlloc( 0, vertexBufferMaxCount * sizeof(TexturedVertex),
                                                                           MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
            u32 indexBufferMaxCount = vertexBufferMaxCount * 8;
            u32 *indexBuffer = (u32 *)VirtualAlloc( 0, indexBufferMaxCount * sizeof(u32),
                                                    MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
            RenderCommands renderCommands = InitRenderCommands( renderBuffer, renderBufferSize,
                                                                    vertexBuffer, vertexBufferMaxCount,
                                                                    indexBuffer, indexBufferMaxCount );

            if( Win32InitOpenGL( deviceContext, renderCommands, frameVSyncSkipCount ) )
            {
                LPVOID baseAddress = 0;

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
                for( u32 replayIndex = 0; replayIndex < ARRAYCOUNT(globalPlatformState.replayBuffers); ++replayIndex )
                {
                    Win32ReplayBuffer *replayBuffer = &globalPlatformState.replayBuffers[replayIndex];

                    // Since we use an index value of 0 as an off flag, slot filenames start at 1
                    Win32GetInputFilePath( globalPlatformState, replayIndex + 1, false,
                                           replayBuffer->filename,
                                           ARRAYCOUNT(replayBuffer->filename) );
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

                    GameInput input[2] = {};
                    GameInput *newInput = &input[0];
                    GameInput *oldInput = &input[1];

                    r32 targetElapsedPerFrameSecs = 1.0f / videoTargetFramerateHz;
                    // Assume our target for the first frame
                    r32 lastDeltaTimeSecs = targetElapsedPerFrameSecs;

                    globalRunning = true;

                    HRESULT audioStarted = globalAudioClient->Start();
                    ASSERT( audioStarted == S_OK );

                    LARGE_INTEGER firstCounter = Win32GetWallClock();
                    LARGE_INTEGER lastCounter = firstCounter;
                    i64 lastCycleCounter = __rdtsc();

                    globalPlatformState.gameCode
                        = Win32LoadGameCode( globalPlatformState.sourceDLLPath, globalPlatformState.tempDLLPath, &gameMemory );
                    Win32SetupAssetUpdateListener( &globalPlatformState );

                    // Main loop
                    while( globalRunning )
                    {
#if !RELEASE
                        DebugFrameInfo DEBUGframeInfo = {};
                        
                        // Prevent huge skips in physics etc. while debugging
                        if( lastDeltaTimeSecs > 1.f )
                        {
                            lastDeltaTimeSecs = targetElapsedPerFrameSecs;
                            totalElapsedSeconds += lastDeltaTimeSecs;
                        }
                        else
#endif
                            totalElapsedSeconds = Win32GetSecondsElapsed( firstCounter, lastCounter );

                        Win32PrepareInputData( oldInput, newInput,
                                               lastDeltaTimeSecs, totalElapsedSeconds, runningFrameCounter );
                        if( runningFrameCounter == 0 )
                            newInput->executableReloaded = true;

#if !RELEASE
                        // Check for game code updates
                        FILETIME dllWriteTime = Win32GetLastWriteTime( globalPlatformState.sourceDLLPath );
                        if( CompareFileTime( &dllWriteTime, &globalPlatformState.gameCode.lastDLLWriteTime ) != 0 )
                        {
                            Win32CompleteAllJobs( globalPlatform.hiPriorityQueue );

                            LOG( "Detected updated game DLL. Reloading.." );
                            Win32UnloadGameCode( &globalPlatformState.gameCode );
                            globalPlatformState.gameCode
                                = Win32LoadGameCode( globalPlatformState.sourceDLLPath, globalPlatformState.tempDLLPath, &gameMemory );
                            newInput->executableReloaded = true;
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
                        Win32ProcessXInputControllers( oldInput, newInput );

#if !RELEASE
                        DEBUGframeInfo.inputProcessedSeconds = Win32GetSecondsElapsed( lastCounter, Win32GetWallClock() );

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
                        io.DeltaTime = lastDeltaTimeSecs;
                        io.DisplaySize.x = (r32)windowDim.width;
                        io.DisplaySize.y = (r32)windowDim.height;
                        io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                        io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                        io.KeySuper = false;
                        ImGui::NewFrame();

                        u32 audioFramesToWrite = 0;
                        u32 audioPaddingFrames;
                        if( SUCCEEDED(globalAudioClient->GetCurrentPadding( &audioPaddingFrames )) )
                        {
                            // TODO Try priming the buffer with something like half a frame's worth of silence

                            audioFramesToWrite = audioOutput.bufferSizeFrames - audioPaddingFrames;
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
                        DEBUGframeInfo.gameUpdatedSeconds = Win32GetSecondsElapsed( lastCounter, Win32GetWallClock() );
#endif

                        // Blit audio buffer to output
                        Win32BlitAudioBuffer( &audioBuffer, audioFramesToWrite, &audioOutput );

#if !RELEASE
                        DEBUGframeInfo.audioUpdatedSeconds = Win32GetSecondsElapsed( lastCounter, Win32GetWallClock() );
#endif

#if 0
                        i64 endCycleCounter = __rdtsc();
                        u64 cyclesElapsed = endCycleCounter - lastCycleCounter;
                        u32 kCyclesElapsed = (u32)(cyclesElapsed / 1000);

                        // Artificially increase wait time from 0 to 20ms.
                        int r = rand() % 20;
                        targetElapsedPerFrameSecs = (1.0f / videoTargetFramerateHz) + ((r32)r / 1000.0f);
#endif
#if 0
                        LARGE_INTEGER endCounter = Win32GetWallClock();
                        r32 frameElapsedSecs = Win32GetSecondsElapsed( lastCounter, endCounter );

                        // Wait till the target frame time
                        r32 elapsedSecs = frameElapsedSecs;
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
                                elapsedSecs = Win32GetSecondsElapsed( lastCounter, Win32GetWallClock() );
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

                        LARGE_INTEGER endCounter = Win32GetWallClock();
                        lastDeltaTimeSecs = Win32GetSecondsElapsed( lastCounter, endCounter );
                        lastCounter = endCounter;

                        ++runningFrameCounter;

#if !RELEASE
                        DEBUGframeInfo.endOfFrameSeconds = lastDeltaTimeSecs;

                        if( globalPlatformState.gameCode.DebugFrameEnd )
                            globalPlatformState.gameCode.DebugFrameEnd( &gameMemory, &DEBUGframeInfo );
#endif

#if 0
                        lastCycleCounter = endCycleCounter;
                        {
                            r32 fps = 1.0f / lastDeltaTimeSecs;
                            LOG( "ms: %.02f - FPS: %.02f (%d Kcycles) - audio padding: %d\n",
                                 1000.0f * lastDeltaTimeSecs, fps, kCyclesElapsed, audioPaddingFrames );
                        }

                        {
                            // Print game debug statistics
                            LOG( ":::Frame counters:" );
                            for( u32 i = 0; i < DEBUGglobalStats->gameCountersCount; ++i )
                            {
                                DebugCycleCounter& c = DEBUGglobalStats->gameCounters[i];

                                u32 frameHitCount = 0;
                                u64 frameCycleCount = 0;
                                UnpackAndResetFrameCounter( c, &frameCycleCount, &frameHitCount );

                                if( frameHitCount > 0 )
                                {
                                    LOG( "%s@%u\t%llu fc  %u h  %u fc/h",
                                         c.function,
                                         c.lineNumber,
                                         frameCycleCount,
                                         frameHitCount,
                                         frameCycleCount/frameHitCount );
                                }
                            }
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

    LOG( "\n\nFPS: %.1f imm. / %.1f avg.",
         ImGui::GetIO().Framerate, (r32)runningFrameCounter / totalElapsedSeconds );

#if !RELEASE
    DebugState* debugState = (DebugState*)gameMemory.debugStorage;

    LOG( ":::Global counters:" );
    for( u32 i = 0; i < debugState->counterLogsCount; ++i )
    {
        DebugCounterLog &log = debugState->counterLogs[i];
        if( log.totalHitCount > 0 )
        {
            LOG( "%s@%u\t%llu tc  %u h  %u tc/h",
                 log.function,
                 log.lineNumber,
                 log.totalCycleCount,
                 log.totalHitCount,
                 log.totalCycleCount/log.totalHitCount );
        }
    }
#endif

    return 0;
}

