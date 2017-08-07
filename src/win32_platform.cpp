#include "robotrider.h"

#include <windows.h>
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

PlatformAPI platform;
OpenGLState openGLState;
global bool globalRunning;
global u32 globalMonitorRefreshHz;
global IAudioClient* globalAudioClient;
global IAudioRenderClient* globalAudioRenderClient;
global i64 globalPerfCounterFrequency;
global HCURSOR DEBUGglobalCursor;
global bool DEBUGglobalShowCursor;


DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if( memory )
    {
        VirtualFree( memory, 0, MEM_RELEASE );
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    DEBUGReadFileResult result = {};

    HANDLE fileHandle = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    if( fileHandle != INVALID_HANDLE_VALUE )
    {
        LARGE_INTEGER fileSize;
        if( GetFileSizeEx( fileHandle, &fileSize ) )
        {
            u32 fileSize32 = SafeTruncToU32( fileSize.QuadPart );
            result.contents = VirtualAlloc( 0, fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            if( result.contents )
            {
                DWORD bytesRead;
                if( ReadFile( fileHandle, result.contents, fileSize32, &bytesRead, 0 )
                    && (fileSize32 == bytesRead) )
                {
                    result.contentSize = fileSize32;
                }
                else
                {
                    // TODO Log
                    DEBUGPlatformFreeFileMemory( result.contents );
                    result.contents = 0;
                }
            }
            else
            {
                // TODO Log
            }
        }
        else
        {
            // TODO Log
        }

        CloseHandle( fileHandle );
    }
    else
    {
        // TODO Log
    }

    return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    b32 result = false;

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
            // TODO Log
        }

        CloseHandle( fileHandle );
    }
    else
    {
        // TODO Log
    }

    return result;
}


PLATFORM_LOG(PlatformLog)
{
    char buffer[1024];

    va_list args;
    va_start( args, fmt );
    vsprintf_s( buffer, ARRAYCOUNT(buffer), fmt, args );
    va_end( args );

    OutputDebugString( buffer );
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
Win32LoadGameCode( char *sourceDLLPath, char *tempDLLPath )
{
    Win32GameCode result = {};
    result.lastDLLWriteTime = Win32GetLastWriteTime( sourceDLLPath );

    CopyFile( sourceDLLPath, tempDLLPath, FALSE );
    result.gameCodeDLL = LoadLibrary( tempDLLPath );
    if( result.gameCodeDLL )
    {
        result.UpdateAndRender = (GameUpdateAndRenderFunc *)GetProcAddress( result.gameCodeDLL, "GameUpdateAndRender" );

        result.isValid = result.UpdateAndRender != 0;
    }

    if( !result.isValid )
    {
        result.UpdateAndRender = GameUpdateAndRenderStub;
    }

    return result;
}

internal void
Win32UnloadGameCode( Win32GameCode *gameCode )
{
    if( gameCode->gameCodeDLL )
    {
        FreeLibrary( gameCode->gameCodeDLL );
        gameCode->gameCodeDLL = 0;
    }

    gameCode->isValid = false;
    gameCode->UpdateAndRender = GameUpdateAndRenderStub;
}


// XInput function pointers and stubs
#define XINPUT_GET_STATE(name) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_STATE *pState )
typedef XINPUT_GET_STATE(XInputGetStateFunc);
XINPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInputGetStateFunc *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define XINPUT_SET_STATE(name) DWORD WINAPI name( DWORD dwUserIndex, XINPUT_VIBRATION* pVibration )
typedef XINPUT_SET_STATE(XInputSetStateFunc);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global XInputSetStateFunc *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void
Win32InitXInput()
{
    HMODULE XInputLibrary = LoadLibrary( "xinput1_4.dll" );
    if( !XInputLibrary )
    {
        // TODO Diagnostic
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
        // TODO Diagnostic
    }
}


// WASAPI functions
internal Win32AudioOutput
Win32InitWASAPI( u32 samplingRate, u16 bitDepth, u16 channelCount, u32 bufferSizeFrames )
{
    if( FAILED(CoInitializeEx( 0, COINIT_SPEED_OVER_MEMORY )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }
    
    IMMDeviceEnumerator *enumerator;
    if( FAILED(CoCreateInstance( __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator) )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    IMMDevice *device;
    if( FAILED(enumerator->GetDefaultAudioEndpoint( eRender, eConsole, &device )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    if( FAILED(device->Activate( __uuidof(IAudioClient), CLSCTX_ALL, NULL, (LPVOID *)&globalAudioClient )) )
    {
        // TODO Diagnostic
        ASSERT( false );
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
    // TODO Diagnostic
    ASSERT( result == S_OK );

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

    if( FAILED(globalAudioClient->GetService( IID_PPV_ARGS(&globalAudioRenderClient) )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    u32 audioFramesCount;
    if( FAILED(globalAudioClient->GetBufferSize( &audioFramesCount )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    ASSERT( bufferSizeFrames == audioFramesCount );

    Win32AudioOutput audioOutput = {};
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
Win32DisplayInWindow( GameRenderCommands &commands, HDC deviceContext, int windowWidth, int windowHeight )
{
    Renderer renderer = Renderer::OpenGL;

    switch( renderer )
    {
        case Renderer::OpenGL:
        {
            OpenGLRenderToOutput( openGLState, commands );
            SwapBuffers( deviceContext );
        } break;

        INVALID_DEFAULT_CASE;
    }

#if 0
    StretchDIBits( deviceContext,
                   0, 0, buffer->width, buffer->height,
                   0, 0, buffer->width, buffer->height,
                   buffer->memory,
                   &buffer->bitmapInfo,
                   DIB_RGB_COLORS,
                   SRCCOPY );
#endif
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
Win32SetButtonState( GameButtonState *newState, b32 isDown )
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
        // FIXME This call stalls for a few hundred thousand cycles when the controller is not present
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

            if( newController->leftStick.avgX != 0.0f || newController->leftStick.avgY != 0.0f )
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
Win32GetInputFilePath( Win32State *platformState, u32 slotIndex, b32 isInputStream,
                       char* dest, u32 destCount )
{
    sprintf_s( dest, destCount, "%s%s%d%s%s", platformState->exeFilePath,
               "gamestate", slotIndex, isInputStream ? "_input" : "", ".in" );
}

internal Win32ReplayBuffer*
Win32GetReplayBuffer( Win32State *platformState, u32 index )
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
        Win32GetInputFilePath( platformState, inputRecordingIndex, true,
                               filename, ARRAYCOUNT(filename) );
        platformState->recordingHandle = CreateFile( filename,
                                                     GENERIC_WRITE, 0, 0,
                                                     CREATE_ALWAYS, 0, 0 );

        CopyMemory( replayBuffer->memoryBlock, platformState->gameMemoryBlock, platformState->gameMemorySize );
        // TODO Write state to disk asynchronously
    }
}

internal void
Win32EndInputRecording( Win32State *platformState )
{
    CloseHandle( platformState->recordingHandle );
    platformState->inputRecordingIndex = 0;
}

internal void
Win32RecordInput( Win32State *platformState, GameInput *newInput )
{
    DWORD bytesWritten;
    WriteFile( platformState->recordingHandle, newInput, sizeof(*newInput),
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
        Win32GetInputFilePath( platformState, inputPlaybackIndex, true,
                               filename, ARRAYCOUNT(filename) );
        platformState->playbackHandle = CreateFile( filename,
                                                    GENERIC_READ, 0, 0,
                                                    OPEN_EXISTING, 0, 0 );

        CopyMemory( platformState->gameMemoryBlock, replayBuffer->memoryBlock, platformState->gameMemorySize );
    }
}

internal void
Win32EndInputPlayback( Win32State *platformState )
{
    CloseHandle( platformState->playbackHandle );
    platformState->inputPlaybackIndex = 0;
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
Win32ProcessPendingMessages( Win32State *platformState, GameInput *input, GameControllerInput *keyMouseController )
{
    MSG message;
    while( PeekMessage( &message, 0, 0, 0, PM_REMOVE ) )
    {
        if( message.message == WM_QUIT)
        {
            globalRunning = false;
        }

        switch( message.message )
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                u32 vkCode = SafeTruncToU32( message.wParam );
                bool wasDown = ((message.lParam & (1 << 30)) != 0);
                bool isDown =  ((message.lParam & (1 << 31)) == 0);
                if( isDown != wasDown )
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
                    else if( vkCode == 'Q' )
                    {
                        Win32SetButtonState( &keyMouseController->leftShoulder, isDown );
                    }
                    else if( vkCode == 'E' )
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
                    else if( vkCode == VK_SPACE )
                    {
                    }
                    else if( vkCode == VK_RETURN )
                    {
                        Win32SetButtonState( &keyMouseController->aButton, isDown );
                    }
#if DEBUG
                    else if( vkCode == VK_ESCAPE )
                    {
                        if( isDown )
                        {
                            if( platformState->inputPlaybackIndex )
                            {
                                Win32EndInputPlayback( platformState );
                                Win32ResetController( keyMouseController );
                            }
                            else
                            {
                                // TODO Change this to two consecutive presses
                                globalRunning = false;
                            }
                        }
                    }
                    // TODO This may only work in the spanish keyboard?
                    else if( vkCode == VK_OEM_5 )
                    {
                        if( isDown )
                        {
                            DEBUGglobalShowCursor = !DEBUGglobalShowCursor;
                            SetCursor( DEBUGglobalShowCursor ? DEBUGglobalCursor : 0 );
                        }
                    }
                    else if( vkCode == '1' )
                    {
                        if( isDown )
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
#endif
                }

                b32 altKeyWasDown = ((message.lParam & (1<<29)) != 0);
                if( vkCode == VK_F4 && altKeyWasDown )
                {
                    globalRunning = false;
                }
            } break;

            //case WM_CHAR:
            //{
                //// TODO Enter text somewhere
            //} break;

#define GET_SIGNED_LO(dw) ((int)(short)LOWORD(dw))
#define GET_SIGNED_HI(dw) ((int)(short)HIWORD(dw))

            case WM_MOUSEMOVE:
            {
                input->mouseX = GET_SIGNED_LO( message.lParam );
                input->mouseY = GET_SIGNED_HI( message.lParam );
                Win32SetButtonState( &input->mouseButtons[0], message.wParam & MK_LBUTTON );
                Win32SetButtonState( &input->mouseButtons[1], message.wParam & MK_MBUTTON );
                Win32SetButtonState( &input->mouseButtons[2], message.wParam & MK_RBUTTON );
                Win32SetButtonState( &input->mouseButtons[3], message.wParam & MK_XBUTTON1 );
                Win32SetButtonState( &input->mouseButtons[4], message.wParam & MK_XBUTTON2 );
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
                Win32SetButtonState( &input->mouseButtons[0], message.wParam & MK_LBUTTON );
                Win32SetButtonState( &input->mouseButtons[1], message.wParam & MK_MBUTTON );
                Win32SetButtonState( &input->mouseButtons[2], message.wParam & MK_RBUTTON );
                Win32SetButtonState( &input->mouseButtons[3], message.wParam & MK_XBUTTON1 );
                Win32SetButtonState( &input->mouseButtons[4], message.wParam & MK_XBUTTON2 );
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

                    b32 bUp, bDown;
                    USHORT buttonFlags = raw->data.mouse.usButtonFlags;

                    bUp = buttonFlags & RI_MOUSE_LEFT_BUTTON_UP;
                    bDown = buttonFlags & RI_MOUSE_LEFT_BUTTON_DOWN;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[0], bDown );
                    }
                    bUp = buttonFlags & RI_MOUSE_MIDDLE_BUTTON_UP;
                    bDown = buttonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[1], bDown );
                    }
                    bUp = buttonFlags & RI_MOUSE_RIGHT_BUTTON_UP;
                    bDown = buttonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[2], bDown );
                    }
                    bUp = buttonFlags & RI_MOUSE_BUTTON_4_UP;
                    bDown = buttonFlags & RI_MOUSE_BUTTON_4_DOWN;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[3], bDown  );
                    }
                    bUp = buttonFlags & RI_MOUSE_BUTTON_5_UP;
                    bDown = buttonFlags & RI_MOUSE_BUTTON_5_DOWN;
                    if( bUp || bDown )
                    {
                        Win32SetButtonState( &input->mouseButtons[4], bDown  );
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


internal void
Win32SetFullscreenWindow( HWND window )
{
    MONITORINFO mi = { sizeof(mi) };

    DWORD dwStyle = GetWindowLong( window, GWL_STYLE );
    if( GetMonitorInfo( MonitorFromWindow( window, MONITOR_DEFAULTTOPRIMARY ), &mi ) )
    {
        SetWindowLong( window, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW );
        SetWindowPos( window, HWND_TOP,
                      mi.rcMonitor.left, mi.rcMonitor.top,
                      mi.rcMonitor.right - mi.rcMonitor.left,
                      mi.rcMonitor.bottom - mi.rcMonitor.top,
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


LRESULT CALLBACK
Win32WindowProc( HWND hwnd,
                 UINT  uMsg,
                 WPARAM wParam,
                 LPARAM lParam )
{
    LRESULT result = 0;
    
    switch(uMsg)
    {
        case WM_ACTIVATEAPP:
        {
            if( wParam == TRUE )
            {
                //SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 255, LWA_ALPHA );
            }
            else
            {
                //SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 128, LWA_ALPHA );
                Win32HideWindow( hwnd );
            }
        } break;

        case WM_KILLFOCUS:
        {
            Win32HideWindow( hwnd );
        } break;

        case WM_SIZE:
        {
        } break;

        case WM_SETCURSOR:
        {
            SetCursor( DEBUGglobalShowCursor ? DEBUGglobalCursor : 0 );
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            ASSERT( !"Received keyboard input in WindowProc!" );
        } break;

        case WM_CLOSE:
        {
            // TODO Message to the user?
            globalRunning = false;
        } break;

        case WM_QUIT:
        {
            // TODO Handle this as an error
            globalRunning = false;
        } break;

        default:
        {
            result = DefWindowProc( hwnd, uMsg, wParam, lParam );
        } break;
    }
    
    return result;
}


internal void
Win32GetExeFilename( Win32State *state )
{
    DWORD pathLen = GetModuleFileName( 0, state->exeFilePath, sizeof(state->exeFilePath) );
    char *onePastLastSlash = state->exeFilePath;
    for( char *scanChar = state->exeFilePath; *scanChar; ++scanChar )
    {
        if( *scanChar == '\\' )
        {
            onePastLastSlash = scanChar + 1;
        }
    }
    if( onePastLastSlash != state->exeFilePath )
    {
        *onePastLastSlash = 0;
    }
}


internal b32
Win32InitOpenGL( HDC dc, u32 frameVSyncSkipCount )
{
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
        // TODO Diagnostic
        return false;
    }

    if( !SetPixelFormat( dc, formatIndex, &pfd ) )
    {
        // TODO Diagnostic
        return false;
    }

    HGLRC legacyContext = wglCreateContext( dc );
    if( !legacyContext )
    {
        int error = glGetError();
        // TODO Diagnostic
        return false;
    }

    if( !wglMakeCurrent( dc, legacyContext ) )
    {
        int error = glGetError();
        // TODO Diagnostic
        return false;
    }

    int flags = WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#if DEBUG
    flags |= WGL_CONTEXT_DEBUG_BIT_ARB;
#endif

    const int contextAttributes[] =
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_FLAGS_ARB, flags,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB
        = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress( "wglCreateContextAttribsARB" );
    if( !wglCreateContextAttribsARB )
    {
        // TODO Diagnostic
        return false;
    }

    HGLRC renderingContext = wglCreateContextAttribsARB( dc, 0, contextAttributes );
    if( !renderingContext )
    {
        int error = glGetError();
        // TODO Diagnostic
        return false;
    }

    // Destroy dummy context
    BOOL res;
    res = wglMakeCurrent( dc, NULL );
    res = wglDeleteContext( legacyContext );

    if( !wglMakeCurrent( dc, renderingContext ) )
    {
        // TODO Diagnostic
        return false;
    }

    // Success
    // Setup pointers to cross platform extension functions
#define BINDGLPROC( name, type ) name = (type)wglGetProcAddress( #name )
    BINDGLPROC( glGetStringi, PFNGLGETSTRINGIPROC );
    BINDGLPROC( glGenBuffers, PFNGLGENBUFFERSPROC );
    BINDGLPROC( glBindBuffer, PFNGLBINDBUFFERPROC );
    BINDGLPROC( glBufferData, PFNGLBUFFERDATAPROC );
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
#undef BINDGLPROC


    OpenGLInfo info = OpenGLInit( true );


    // VSync
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress( "wglSwapIntervalEXT" );

    if( wglSwapIntervalEXT )
    {
        wglSwapIntervalEXT( frameVSyncSkipCount );
    }

    return true;
}



int CALLBACK
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nCmdShow )
{

    GameMemory gameMemory = {};
    gameMemory.permanentStorageSize = MEGABYTES(64);
    gameMemory.transientStorageSize = GIGABYTES((u64)1);
    gameMemory.platformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
    gameMemory.platformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
    gameMemory.platformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
    gameMemory.platformAPI.Log = PlatformLog;

    platform = gameMemory.platformAPI;

    Win32State platformState = {};
    Win32GetExeFilename( &platformState );

    char *sourceDLLName = "robotrider.dll";
    char sourceDLLPath[MAX_PATH];
    sprintf_s( sourceDLLPath, ARRAYCOUNT(sourceDLLPath), "%s%s", platformState.exeFilePath, sourceDLLName );
    char *tempDLLName = "robotrider_temp.dll";
    char tempDLLPath[MAX_PATH];
    sprintf_s( tempDLLPath, ARRAYCOUNT(tempDLLPath), "%s%s", platformState.exeFilePath, tempDLLName );

    // Init subsystems
    Win32InitXInput();
    // TODO Test what a safe value for buffer size/latency is with several audio cards
    // (stress test by artificially lowering the framerate)
    Win32AudioOutput audioOutput = Win32InitWASAPI( 48000, AUDIO_BITDEPTH, AUDIO_CHANNELS, AUDIO_LATENCY_SAMPLES );

    LARGE_INTEGER perfCounterFreqMeasure;
    QueryPerformanceFrequency( &perfCounterFreqMeasure );
    globalPerfCounterFrequency = perfCounterFreqMeasure.QuadPart;

    // Set Windows schduler granularity so that our frame wait sleep is more granular
    b32 sleepIsGranular = (timeBeginPeriod( 1 ) == TIMERR_NOERROR);
    if( !sleepIsGranular )
    {
        // TODO Log a bit fat warning
    }

    // Register window class and create game window
    WNDCLASS windowClass = {};
    windowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    windowClass.lpfnWndProc = Win32WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = "RobotRiderWindowClass";

    if( RegisterClass( &windowClass ) )
    {
        HWND window = CreateWindowEx( 0, //WS_EX_TOPMOST|WS_EX_LAYERED,
                                      windowClass.lpszClassName,
                                      "RobotRider",
                                      WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      CW_USEDEFAULT,
                                      0,
                                      0,
                                      hInstance,
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
                LOG( "Monitor refresh rate: %d Hz\n", globalMonitorRefreshHz );
            }
            else
            {
                LOG( "WNG: Failed to query monitor refresh rate. Using %d Hz\n", VIDEO_TARGET_FRAMERATE );
            }
            u32 frameVSyncSkipCount = Round( (r32)globalMonitorRefreshHz / VIDEO_TARGET_FRAMERATE );
            u32 videoTargetFramerateHz = globalMonitorRefreshHz / frameVSyncSkipCount;

            // Determine system latency
            REFERENCE_TIME latency;
            globalAudioClient->GetStreamLatency( &latency );
            audioOutput.systemLatencyFrames = (u16)Ceil( (u64)latency * audioOutput.samplingRate / 10000000.0 );
            u32 audioLatencyFrames = audioOutput.samplingRate / videoTargetFramerateHz;

#if !DEBUG
            Win32SetFullscreenWindow( window );
#endif
            DEBUGglobalCursor = LoadCursor( 0, IDC_CROSS );

            platformState.mainWindow = window;
            Win32RegisterRawMouseInput( window );

            if( Win32InitOpenGL( deviceContext, frameVSyncSkipCount ) )
            {
                LPVOID baseAddress = 0;
#if DEBUG
                baseAddress = (LPVOID)GIGABYTES((u64)2048);
#endif
                // Allocate game memory pools
                u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
                // TODO Use MEM_LARGE_PAGES and call AdjustTokenPrivileges when not in XP
                gameMemory.permanentStorage = VirtualAlloc( baseAddress, totalSize,
                                                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
                gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage
                    + gameMemory.permanentStorageSize;

                platformState.gameMemoryBlock = gameMemory.permanentStorage;
                platformState.gameMemorySize = totalSize;

                i16 *soundSamples = (i16 *)VirtualAlloc( 0, audioOutput.bufferSizeFrames*audioOutput.bytesPerFrame,
                                                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

#if DEBUG
                for( int replayIndex = 0; replayIndex < ARRAYCOUNT(platformState.replayBuffers); ++replayIndex )
                {
                    Win32ReplayBuffer *replayBuffer = &platformState.replayBuffers[replayIndex];

                    // Since we use an index value of 0 as an off flag, slot filenames start at 1
                    Win32GetInputFilePath( &platformState, replayIndex + 1, false,
                                           replayBuffer->filename,
                                           ARRAYCOUNT(replayBuffer->filename) );
                    replayBuffer->fileHandle = CreateFile( replayBuffer->filename,
                                                           GENERIC_READ|GENERIC_WRITE, 0, 0,
                                                           CREATE_ALWAYS, 0, 0 );
                    DWORD ignored;
                    DeviceIoControl( platformState.recordingHandle, FSCTL_SET_SPARSE, 0, 0, 0, 0, &ignored, 0 );

                    replayBuffer->memoryMap = CreateFileMapping( replayBuffer->fileHandle,
                                                                 0, PAGE_READWRITE,
                                                                 (platformState.gameMemorySize >> 32),
                                                                 platformState.gameMemorySize & 0xFFFFFFFF,
                                                                 0 );

                    replayBuffer->memoryBlock = MapViewOfFile( replayBuffer->memoryMap,
                                                               FILE_MAP_ALL_ACCESS,
                                                               0, 0, platformState.gameMemorySize );
                    if( !replayBuffer->memoryBlock )
                    {
                        // TODO Diagnostic
                    }
                }
#endif

                if( gameMemory.permanentStorage && gameMemory.transientStorage && soundSamples )
                {
                    GameInput input[2] = {};
                    GameInput *newInput = &input[0];
                    GameInput *oldInput = &input[1];

                    r32 targetElapsedPerFrameSecs = 1.0f / videoTargetFramerateHz;
                    // Assume our target for the first frame
                    r32 lastDeltaTimeSecs = targetElapsedPerFrameSecs;

                    globalRunning = true;

                    HRESULT audioStarted = globalAudioClient->Start();
                    ASSERT( audioStarted == S_OK );

                    LARGE_INTEGER lastCounter = Win32GetWallClock();
                    i64 lastCycleCounter = __rdtsc();

                    Win32GameCode game = Win32LoadGameCode( sourceDLLPath, tempDLLPath );

                    u32 runningFrameCounter = 0;
                    while( globalRunning )
                    {
                        newInput->secondsElapsed = lastDeltaTimeSecs;

                        FILETIME dllWriteTime = Win32GetLastWriteTime( sourceDLLPath );
                        if( CompareFileTime( &dllWriteTime, &game.lastDLLWriteTime ) != 0 )
                        {
                            Win32UnloadGameCode( &game );
                            game = Win32LoadGameCode( sourceDLLPath, tempDLLPath );
                        }

                        // Process input
                        GameControllerInput *newKeyMouseController = Win32ResetKeyMouseController( oldInput, newInput );
                        Win32ProcessPendingMessages( &platformState, newInput, newKeyMouseController );
                        Win32ProcessXInputControllers( oldInput, newInput );

                        if( platformState.inputRecordingIndex )
                        {
                            Win32RecordInput( &platformState, newInput );
                        }
                        if( platformState.inputPlaybackIndex )
                        {
                            Win32PlayBackInput( &platformState, newInput );
                        }

                        u32 framesToWrite = 0;
                        u32 audioPaddingFrames;
                        if( SUCCEEDED(globalAudioClient->GetCurrentPadding( &audioPaddingFrames )) )
                        {
                            // TODO Try priming the buffer with something like half a frame's worth of silence

                            framesToWrite = audioOutput.bufferSizeFrames - audioPaddingFrames;
                        }

                        // Prepare audio & video buffers
                        GameAudioBuffer audioBuffer = {};
                        audioBuffer.samplesPerSecond = audioOutput.samplingRate;
                        audioBuffer.channelCount = AUDIO_CHANNELS;
                        audioBuffer.frameCount = framesToWrite;
                        audioBuffer.samples = soundSamples;

                        Win32WindowDimension windowDim = Win32GetWindowDimension( window );
                        GameRenderCommands renderCommands = {};
                        renderCommands.width = (u16)windowDim.width;
                        renderCommands.height = (u16)windowDim.height;

                        // Ask the game to render one frame
                        game.UpdateAndRender( &gameMemory, newInput, renderCommands, &audioBuffer );

                        // Blit audio buffer to output
                        Win32BlitAudioBuffer( &audioBuffer, framesToWrite, &audioOutput );

                        GameInput *temp = newInput;
                        newInput = oldInput;
                        oldInput = temp;

                        i64 endCycleCounter = __rdtsc();
                        u64 cyclesElapsed = endCycleCounter - lastCycleCounter;
                        u32 kCyclesElapsed = (u32)(cyclesElapsed / 1000);

#if 0
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
                        Win32DisplayInWindow( renderCommands, deviceContext, windowDim.width, windowDim.height );

                        LARGE_INTEGER endCounter = Win32GetWallClock();
                        lastDeltaTimeSecs = Win32GetSecondsElapsed( lastCounter, endCounter );
                        lastCounter = endCounter;

                        lastCycleCounter = endCycleCounter;
                        ++runningFrameCounter;
#if 1
                        {
                            r32 fps = 1.0f / lastDeltaTimeSecs;
                            LOG( "ms: %.02f - FPS: %.02f (%d Kcycles) - audio padding: %d\n",
                                 1000.0f * lastDeltaTimeSecs, fps, kCyclesElapsed, audioPaddingFrames );
                        }
#endif
                    }
                }
                else
                {
                    // TODO Log "Couldn't allocate memory"
                }
            }
            else
            {
                LOG( "OpenGL initialization failed" );
            }
        }
        else
        {
            // TODO Log "Couldn't create window"
        }
    }
    else
    {
        // TODO Log "Couldn't register window class"
    }
    return 0;
}

