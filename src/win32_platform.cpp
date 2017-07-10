#include "robotrider.h"

#include <windows.h>
#include <gl/gl.h>
#include "wglext.h"
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

global bool globalRunning;
global IAudioClient* globalAudioClient;
global IAudioRenderClient* globalAudioRenderClient;
global s64 globalPerfCounterFrequency;


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
            u32 fileSize32 = SafeTruncU64( fileSize.QuadPart );
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
        s16* sourceSample = sourceBuffer->samples;
        s16* destSample = (s16*)audioData;
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
Win32DisplayInWindow( GameRenderCommands *commands, HDC deviceContext, int windowWidth, int windowHeight )
{
    Renderer renderer = Renderer::OpenGL;

    switch( renderer )
    {
        case Renderer::OpenGL:
        {
            OpenGLRenderToOutput( commands );
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


internal GameControllerInput*
Win32ResetKeyboardController( GameInput* oldInput, GameInput* newInput )
{
    GameControllerInput *oldKeyboardController = GetController( oldInput, 0 );
    GameControllerInput *newKeyboardController = GetController( newInput, 0 );

    *newKeyboardController = {};
    newKeyboardController->isConnected = true;

    newKeyboardController->leftStick = oldKeyboardController->leftStick;
    newKeyboardController->rightStick = oldKeyboardController->rightStick;

    for( int buttonIndex = 0;
         buttonIndex < ARRAYCOUNT( newKeyboardController->buttons );
         ++buttonIndex )
    {
        newKeyboardController->buttons[buttonIndex].endedDown =
            oldKeyboardController->buttons[buttonIndex].endedDown;
    }

    return newKeyboardController;
}

internal void
Win32ResetKeyboardController( GameControllerInput *keyboardController )
{
    *keyboardController = {};
    keyboardController->isConnected = true;
}

internal void
Win32ProcessKeyboardMessage( GameButtonState *newState, b32 isDown )
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


internal void
Win32ProcessPendingMessages( Win32State *platformState, GameControllerInput *keyboardController )
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
                u32 vkCode = SafeTruncU64( message.wParam );
                bool wasDown = ((message.lParam & (1 << 30)) != 0);
                bool isDown =  ((message.lParam & (1 << 31)) == 0);
                if( isDown != wasDown )
                {
                    if( vkCode == 'W' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dUp, isDown );
                        // Simulate fake stick
                        keyboardController->leftStick.avgY += isDown ? 0.5f : -0.5f;
                    }
                    else if( vkCode == 'A' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dLeft, isDown );
                        keyboardController->leftStick.avgX += isDown ? -0.5f : 0.5f;
                    }
                    else if( vkCode == 'S' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dDown, isDown );
                        keyboardController->leftStick.avgY += isDown ? -0.5f : 0.5f;
                    }
                    else if( vkCode == 'D' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dRight, isDown );
                        keyboardController->leftStick.avgX += isDown ? 0.5f : -0.5f;
                    }
                    else if( vkCode == 'Q' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->leftShoulder, isDown );
                    }
                    else if( vkCode == 'E' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->rightShoulder, isDown );
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
                        Win32ProcessKeyboardMessage( &keyboardController->aButton, isDown );
                    }
                    else if( vkCode == VK_ESCAPE )
                    {
                        if( isDown )
                        {
#if DEBUG
                            if( platformState->inputPlaybackIndex )
                            {
                                Win32EndInputPlayback( platformState );
                                Win32ResetKeyboardController( keyboardController );
                            }
                            else
#endif
                            {
                                globalRunning = false;
                            }
                        }
                    }

#if DEBUG
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

            default:
            {
                TranslateMessage( &message );
                DispatchMessage( &message );
            } break;
        }
    }
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
                SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 255, LWA_ALPHA );
            }
            else
            {
                SetLayeredWindowAttributes( hwnd, RGB( 0, 0, 0 ), 128, LWA_ALPHA );
            }
        } break;

        case WM_SIZE:
        {
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
Win32InitOpenGL( HDC dc )
{
    b32 result = false;

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

    if( formatIndex )
    {
        if( SetPixelFormat( dc, formatIndex, &pfd ) )
        {
            HGLRC renderingContext = wglCreateContext( dc );

            if( renderingContext )
            {
                if( wglMakeCurrent( dc, renderingContext ) )
                {
                    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB
                        = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress( "wglGetExtensionsStringARB" );

                    if( wglGetExtensionsStringARB )
                    {
                        // Parse available extensions
                        char *extensionsString = (char *)wglGetExtensionsStringARB( dc );

                        char *extensionsArray[1024];
                        u32 extensionIndex = 0;

                        char *nextToken = NULL;
                        char *str = strtok_s( extensionsString, " ", &nextToken );
                        while( str != NULL )
                        {
                            extensionsArray[extensionIndex++] = str;
                            ASSERT( extensionIndex < ARRAYCOUNT(extensionsArray) );

                            str = strtok_s( NULL, " ", &nextToken );
                        }

                        //if( Win32FindExtensionString( "WGL_ARB_pixel_format" ) )
                        {
                            PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB
                                = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress( "wglChoosePixelFormatARB" );

                            const int pixelFormatAttributes[] =
                            {
                                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                                WGL_COLOR_BITS_ARB, 32,
                                WGL_DEPTH_BITS_ARB, 24,
                                WGL_STENCIL_BITS_ARB, 8,
                                0,        //End
                            };

                            int pixelFormat;
                            UINT numFormats;
                            if( wglChoosePixelFormatARB( dc, pixelFormatAttributes, NULL, 1, &pixelFormat, &numFormats ) )
                            {
                                if( SetPixelFormat( dc, pixelFormat, &pfd ) )
                                {
                                    int flags = 0; // TODO WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#ifdef DEBUG
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

                                    if( wglCreateContextAttribsARB )
                                    {
                                        // Destroy dummy context
                                        BOOL res;
                                        res = wglMakeCurrent( dc, NULL );
                                        res = wglDeleteContext( renderingContext );

                                        renderingContext = wglCreateContextAttribsARB( dc, 0, contextAttributes );

                                        if( renderingContext )
                                        {
                                            if( wglMakeCurrent( dc, renderingContext ) )
                                            {
                                                result = true;
                                            }
                                            else
                                            {
                                                // TODO 
                                            }
                                        }
                                        else
                                        {
                                            int error = glGetError();
                                            int bla = 5;
                                            // TODO
                                        }
                                    }
                                    else
                                    {
                                        // TODO
                                    }
                                }
                                else
                                {
                                    // TODO Diagnostic
                                }
                            }
                            else
                            {
                                // TODO Diagnostic
                            }
                        }
                        //else
                        {
                            // TODO Diagnostic
                        }
                    }
                    else
                    {
                        // TODO Diagnostic
                    }
                }
            }
            else
            {
                // TODO Diagnostic
            }
        }
        else
        {
            // TODO Diagnostic
        }
    }
    else
    {
        // TODO Diagnostic
    }

    return result;
}


int CALLBACK
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nCmdShow )
{
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

    // Determine system latency
    REFERENCE_TIME latency;
    globalAudioClient->GetStreamLatency( &latency );
    audioOutput.systemLatencyFrames = (u16)Ceil( (u64)latency * audioOutput.samplingRate / 10000000.0 );
    u32 audioLatencyFrames = audioOutput.samplingRate / VIDEO_TARGET_FRAMERATE;

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
            if( Win32InitOpenGL( deviceContext ) )
            {
                LPVOID baseAddress = DEBUG ? (LPVOID)GIGABYTES((u64)2048) : 0;

                GameMemory gameMemory = {};
                gameMemory.permanentStorageSize = MEGABYTES(64);
                gameMemory.transientStorageSize = GIGABYTES((u64)1);
                gameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
                gameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
                gameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

                u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
                // TODO Use MEM_LARGE_PAGES and call AdjustTokenPrivileges when not in XP
                gameMemory.permanentStorage = VirtualAlloc( baseAddress, totalSize,
                                                            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
                gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage
                    + gameMemory.permanentStorageSize;

                platformState.gameMemoryBlock = gameMemory.permanentStorage;
                platformState.gameMemorySize = totalSize;

                GameRenderCommands renderCommands;
                // TODO 

                s16 *soundSamples = (s16 *)VirtualAlloc( 0, audioOutput.bufferSizeFrames*audioOutput.bytesPerFrame,
                                                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

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

                if( gameMemory.permanentStorage && gameMemory.transientStorage && soundSamples )
                {
                    GameInput input[2] = {};
                    GameInput *newInput = &input[0];
                    GameInput *oldInput = &input[1];

                    r32 targetElapsedPerFrameSecs = 1.0f / VIDEO_TARGET_FRAMERATE;
                    // Assume our target for the first frame
                    r32 lastDeltaTimeSecs = targetElapsedPerFrameSecs;

                    globalRunning = true;

                    ASSERT( globalAudioClient->Start() == S_OK );

                    LARGE_INTEGER lastCounter = Win32GetWallClock();
                    s64 lastCycleCounter = __rdtsc();

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
                        GameControllerInput *newKeyboardController = Win32ResetKeyboardController( oldInput, newInput );
                        Win32ProcessPendingMessages( &platformState, newKeyboardController );

                        Win32ProcessXInputControllers( oldInput, newInput );

                        POINT mouseP;
                        GetCursorPos( &mouseP );
                        ScreenToClient( window, &mouseP );
                        newInput->mouseX = mouseP.x;
                        newInput->mouseY = mouseP.y;
                        Win32ProcessKeyboardMessage( &newInput->mouseButtons[0], GetKeyState( VK_LBUTTON ) & (1 << 15) );
                        Win32ProcessKeyboardMessage( &newInput->mouseButtons[1], GetKeyState( VK_MBUTTON ) & (1 << 15) );
                        Win32ProcessKeyboardMessage( &newInput->mouseButtons[2], GetKeyState( VK_RBUTTON ) & (1 << 15) );
                        Win32ProcessKeyboardMessage( &newInput->mouseButtons[3], GetKeyState( VK_XBUTTON1 ) & (1 << 15) );
                        Win32ProcessKeyboardMessage( &newInput->mouseButtons[4], GetKeyState( VK_XBUTTON2 ) & (1 << 15) );

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

                        // Ask the game to render one frame
                        game.UpdateAndRender( &gameMemory, newInput, &renderCommands, &audioBuffer );

                        // Blit audio buffer to output
                        Win32BlitAudioBuffer( &audioBuffer, framesToWrite, &audioOutput );

                        GameInput *temp = newInput;
                        newInput = oldInput;
                        oldInput = temp;

                        s64 endCycleCounter = __rdtsc();
                        u64 cyclesElapsed = endCycleCounter - lastCycleCounter;
                        u32 kCyclesElapsed = (u32)(cyclesElapsed / 1000);

                        LARGE_INTEGER endCounter = Win32GetWallClock();
                        r32 frameElapsedSecs = Win32GetSecondsElapsed( lastCounter, endCounter );

#if 0
                        // Artificially increase wait time from 0 to 20ms.
                        int r = rand() % 20;
                        targetElapsedPerFrameSecs = (1.0f / VIDEO_TARGET_FRAMERATE) + ((r32)r / 1000.0f);
#endif
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

                        endCounter = Win32GetWallClock();
                        lastDeltaTimeSecs = Win32GetSecondsElapsed( lastCounter, endCounter );
                        lastCounter = endCounter;

                        // Blit video to output
                        Win32WindowDimension dim = Win32GetWindowDimension( window );
                        Win32DisplayInWindow( &renderCommands, deviceContext, dim.width, dim.height );

                        lastCycleCounter = endCycleCounter;
                        ++runningFrameCounter;
#if 0
                        {
                            r32 fps = 1.0f / lastDeltaTimeSecs;
                            char buffer[256];
                            sprintf_s( buffer, ARRAYCOUNT( buffer ), "ms: %.02f - FPS: %.02f (%d Kcycles) - audio padding: %d\n",
                                       1000.0f * lastDeltaTimeSecs, fps, kCyclesElapsed, audioPaddingFrames );
                            OutputDebugString( buffer );
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
                // TODO Log "OpenGL initialization failed"
                OutputDebugString( "BOOOOO" );
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

