#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>

#define global static
#define internal static
#define local_persistent static

#define PI32 3.141592653589f
#define VIDEO_TARGET_FRAMERATE 60
#define AUDIO_BITDEPTH 16
#define AUDIO_CHANNELS 2


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;
typedef float r32;
typedef double r64;

#include "robotrider.cpp"

#include "win32_platform.h"

global bool globalRunning;
global Win32OffscreenBuffer globalBackBuffer;
global LPDIRECTSOUNDBUFFER globalSecondaryBuffer;
global IAudioClient* globalAudioClient;
global IAudioRenderClient* globalAudioRenderClient;
global s64 globalPerfCounterFrequency;

internal DEBUGReadFileResult
DEBUGPlatformReadEntireFile( char *filename )
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

internal void
DEBUGPlatformFreeFileMemory( void *memory )
{
    if( memory )
    {
        VirtualFree( memory, 0, MEM_RELEASE );
    }
}

internal b32
DEBUGPlatformWriteEntireFile( char*filename, u32 memorySize, void *memory )
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
internal void
Win32InitWASAPI( u32 samplingRate, u16 bitsPerSample, u16 channelCount, u32 bufferSizeSamples )
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
    waveFormat.Format.wBitsPerSample = bitsPerSample;
    waveFormat.Format.nBlockAlign = (WORD)(waveFormat.Format.nChannels * waveFormat.Format.wBitsPerSample / 8);
    waveFormat.Format.nAvgBytesPerSec = waveFormat.Format.nSamplesPerSec * waveFormat.Format.nBlockAlign;
    waveFormat.Samples.wValidBitsPerSample = bitsPerSample;
    waveFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    waveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;

    REFERENCE_TIME bufferDuration = 10000000ULL * bufferSizeSamples / samplingRate; // buffer size in hundreds of nanoseconds
    HRESULT result = globalAudioClient->Initialize( AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_NOPERSIST,
                                              bufferDuration, 0, &waveFormat.Format, nullptr );
	if( result != S_OK )
    {
        // TODO Diagnostic
        ASSERT( false && result );
    }

    if( FAILED(globalAudioClient->GetService( IID_PPV_ARGS(&globalAudioRenderClient) )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    u32 soundFrameCount;
    if( FAILED(globalAudioClient->GetBufferSize( &soundFrameCount )) )
    {
        // TODO Diagnostic
        ASSERT( false );
    }

    ASSERT( bufferSizeSamples <= soundFrameCount );
}



// DirectSound function pointers and stubs
#define DIRECTSOUND_CREATE(name) HRESULT WINAPI name( LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter )
typedef DIRECTSOUND_CREATE(DirectSoundCreateFunc);

internal void
Win32InitDirectSound( HWND window, u32 samplesPerSecond, u32 bufferSize )
{
    HMODULE DSoundLibrary = LoadLibrary( "dsound.dll" );

    if( DSoundLibrary )
    {
        DirectSoundCreateFunc *DirectSoundCreate
            = (DirectSoundCreateFunc *)GetProcAddress( DSoundLibrary, "DirectSoundCreate" ); 

        LPDIRECTSOUND directSound;
        if( DirectSoundCreate && SUCCEEDED(DirectSoundCreate( 0, &directSound, 0)) )
        {
            if( SUCCEEDED(directSound->SetCooperativeLevel( window, DSSCL_PRIORITY )) )
            {
                DSBUFFERDESC primaryDesc = {};
                primaryDesc.dwSize = sizeof(primaryDesc);
                primaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // TODO DSBAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER primaryBuffer;
                if( SUCCEEDED(directSound->CreateSoundBuffer( &primaryDesc, &primaryBuffer, 0 )) )
                {
                    WAVEFORMATEX waveFormat = {};
                    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
                    waveFormat.nChannels = 2;
                    waveFormat.nSamplesPerSec = samplesPerSecond;
                    waveFormat.wBitsPerSample = 16;
                    waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
                    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
                    waveFormat.cbSize = 0;

                    if( SUCCEEDED(primaryBuffer->SetFormat( &waveFormat )) )
                    {
                        DSBUFFERDESC secondaryDesc = {};
                        secondaryDesc.dwSize = sizeof(secondaryDesc);
                        secondaryDesc.dwFlags = 0;
                        secondaryDesc.dwBufferBytes = bufferSize;
                        secondaryDesc.lpwfxFormat = &waveFormat;

                        if( SUCCEEDED(directSound->CreateSoundBuffer( &secondaryDesc, &globalSecondaryBuffer, 0 )) )
                        {
                            // TODO Diagnostic
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
}

internal void
Win32BlitAudioBuffer( GameAudioBuffer *sourceBuffer, u32 framesToWrite, Win32AudioOutput *audioOutput )
{
    /*
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;

    if( SUCCEEDED(globalSecondaryBuffer->Lock( byteToLock, bytesToWrite,
                                               &region1, &region1Size,
                                               &region2, &region2Size,
                                               0 )) )
    {
        s16 *sourceSample = sourceBuffer->samples;

        // TODO Assert that region1Size/region2Size are valid
        s16 *destSample = (s16 *)region1;
        DWORD region1SampleCount = region1Size / audioOutput->bytesPerFrame;
        for( DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++audioOutput->writePositionFrames;
        }

        destSample = (s16 *)region2;
        DWORD region2SampleCount = region2Size / audioOutput->bytesPerFrame;
        for( DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++audioOutput->writePositionFrames;
        }
        globalSecondaryBuffer->Unlock( region1, region1Size, region2, region2Size );
    }
    */
    BYTE *audioData;
    if( SUCCEEDED(globalAudioRenderClient->GetBuffer( framesToWrite, &audioData )) )
    {
        s16* sourceSample = sourceBuffer->samples;
        s16* destSample = (s16*)audioData;
        for( u32 frameIndex = 0; frameIndex < framesToWrite; ++frameIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++audioOutput->writePositionFrames;
        }

        globalAudioRenderClient->ReleaseBuffer( framesToWrite, 0 );
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
Win32DisplayInWindow( Win32OffscreenBuffer *buffer, HDC deviceContext, int windowWidth, int windowHeight )
{
    StretchDIBits( deviceContext,
                   0, 0, windowWidth, windowHeight,
                   0, 0, buffer->width, buffer->height,
                   buffer->memory,
                   &buffer->bitmapInfo,
                   DIB_RGB_COLORS,
                   SRCCOPY );
}

internal GameControllerInput*
Win32ResetKeyboardController( GameInput* oldInput, GameInput* newInput )
{
    GameControllerInput *oldKeyboardController = GetController( oldInput, 0 );
    GameControllerInput *newKeyboardController = GetController( newInput, 0 );
    *newKeyboardController = {};
    newKeyboardController->isConnected = true;

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
Win32ProcessKeyboardMessage( GameButtonState *newState, b32 isDown )
{
    ASSERT( newState->endedDown != isDown );
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
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
            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

            newController->isAnalog = true;

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
        }
        else
        {
            // Controller not available
            newController->isConnected = false;
        }
    }

}

internal void Win32ProcessPendingMessages( GameControllerInput *keyboardController )
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
                    }
                    else if( vkCode == 'A' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dLeft, isDown );
                    }
                    else if( vkCode == 'S' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dDown, isDown );
                    }
                    else if( vkCode == 'D' )
                    {
                        Win32ProcessKeyboardMessage( &keyboardController->dRight, isDown );
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
                    else if( vkCode == VK_ESCAPE )
                    {
                        globalRunning = false;
                    }
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
        case WM_SIZE:
        {
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint( hwnd, &paint );
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            
            Win32WindowDimension dim = Win32GetWindowDimension( hwnd );
            Win32DisplayInWindow( &globalBackBuffer, deviceContext, dim.width, dim.height );
            EndPaint( hwnd, &paint );
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

inline u32
Ceil( r64 value )
{
    u32 result = (u32)(value + 0.5);
    return result;
}

int CALLBACK
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nCmdShow )
{
    Win32AudioOutput audioOutput = {};
    audioOutput.samplingRate = 48000;
    audioOutput.bytesPerFrame = AUDIO_BITDEPTH * AUDIO_CHANNELS / 8;
    audioOutput.bufferSizeFrames = audioOutput.samplingRate;            // 1 sec.
    audioOutput.writePositionFrames = 0;

    // Init subsystems
    Win32InitXInput();
    Win32AllocateBackBuffer( &globalBackBuffer, 1280, 720 );
    // TODO Determine appropriate buffer size
    Win32InitWASAPI( audioOutput.samplingRate, AUDIO_BITDEPTH, AUDIO_CHANNELS, audioOutput.samplingRate/2 );
    // Determine system latency
    REFERENCE_TIME latency;
    globalAudioClient->GetStreamLatency( &latency );
    u32 audioFramesPerSec = audioOutput.samplingRate;
    audioOutput.systemLatencyFrames = Ceil( (u64)latency * audioFramesPerSec / 10000000.0 );

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
    windowClass.style = CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = Win32WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = "RobotRiderWindowClass";

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
                                      hInstance,
                                      0 );
                                            
        if( window )
        {
            globalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING );

            s16 *soundSamples = (s16 *)VirtualAlloc( 0, audioOutput.bufferSizeFrames*audioOutput.bytesPerFrame,
                                                     MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

            LPVOID baseAddress = DEBUG ? (LPVOID)GIGABYTES((u64)2048) : 0;

            GameMemory gameMemory = {};
            gameMemory.permanentStorageSize = MEGABYTES(64);
            gameMemory.transientStorageSize = GIGABYTES((u64)4);

            u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            gameMemory.permanentStorage = VirtualAlloc( baseAddress, totalSize,
                                                        MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
            gameMemory.transientStorage = (u8 *)gameMemory.permanentStorage
                + gameMemory.permanentStorageSize;

            if( gameMemory.permanentStorage && gameMemory.transientStorage && soundSamples )
            {
                GameInput input[2] = {};
                GameInput *newInput = &input[0];
                GameInput *oldInput = &input[1];

                r32 targetElapsedPerFrameSecs = 1.0f / VIDEO_TARGET_FRAMERATE;

                HDC deviceContext = GetDC( window );
                globalRunning = true;

                LARGE_INTEGER lastCounter = Win32GetWallClock();
                s64 lastCycleCounter = __rdtsc();

                while( globalRunning )
                {
                    // Process input
                    GameControllerInput *newKeyboardController = Win32ResetKeyboardController( oldInput, newInput );
                    Win32ProcessPendingMessages( newKeyboardController );

                    Win32ProcessXInputControllers( oldInput, newInput );

                    DWORD byteToLock = 0;
                    DWORD bytesToWrite = 0;
                    DWORD targetCursor = 0;
                    DWORD playCursor, writeCursor;

                    // Figure out how many frames of audio to write in the next update
                    // TODO In the future, when we pass frame delta time to the game, this calculation won't be needed
                    // (will always be the amount of time that passed, if we didn't reach the framerate,  else the framerate time)
                    /*
                    if( SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition( &playCursor, &writeCursor )) )
                    {
                        byteToLock = (audioOutput.writePositionSamples * audioOutput.bytesPerFrame)
                            % audioOutput.bufferSizeBytes;
                        targetCursor = (playCursor + (audioOutput.latencySamples * audioOutput.bytesPerFrame))
                            % audioOutput.bufferSizeBytes;

                        if( byteToLock > targetCursor )
                        {
                            bytesToWrite = audioOutput.bufferSizeBytes - byteToLock;
                            bytesToWrite += targetCursor;
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }
                    }
                    */
                    u32 framesToWrite = 0;
                    u32 audioPaddingFrames;
                    if( SUCCEEDED(globalAudioClient->GetCurrentPadding( &audioPaddingFrames )) )
                    {
                        framesToWrite = audioOutput.bufferSizeFrames - audioPaddingFrames;
                        if( framesToWrite < audioOutput.systemLatencyFrames )
                        {
                            // TODO Log "available space in audio buffer is less than system latency"
                        }
                    }

                    // Prepare audio & video buffers
                    GameOffscreenBuffer videoBuffer = {};
                    videoBuffer.memory = globalBackBuffer.memory;
                    videoBuffer.width = globalBackBuffer.width;
                    videoBuffer.height = globalBackBuffer.height;
                    videoBuffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;

                    GameAudioBuffer audioBuffer = {};
                    audioBuffer.samplesPerSecond = audioOutput.samplingRate;
                    // TODO In the future, when we pass frame delta time to the game, frameCount won't be needed
                    audioBuffer.frameCount = bytesToWrite / audioOutput.bytesPerFrame;
                    audioBuffer.samples = soundSamples;

                    // Ask the game to render one frame
                    GameUpdateAndRender( &gameMemory, newInput, &videoBuffer, &audioBuffer );

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
#if 0
                            {
                                char buffer[256];
                                sprintf_s( buffer, ARRAYCOUNT( buffer ), "frameElapsedMs: %.02f - sleep for %dms.\n", frameElapsedSecs * 1000.0f, sleepMs );
                                OutputDebugString( buffer );
                            }
#endif
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

                    // Blit video to output
                    Win32WindowDimension dim = Win32GetWindowDimension( window );
                    Win32DisplayInWindow( &globalBackBuffer, deviceContext, dim.width, dim.height );

#if 1
                    r32 fps = 1.0f / elapsedSecs;
                    {
                        char buffer[256];
                        sprintf_s( buffer, ARRAYCOUNT( buffer ), "ms: %.02f - FPS: %.02f (%d Kcycles)\n", 1000.0f * elapsedSecs, fps, kCyclesElapsed );
                        OutputDebugString( buffer );
                    }
#endif
                    endCounter = Win32GetWallClock();
                    lastCounter = endCounter;
                    lastCycleCounter = endCycleCounter;
                }
            }
            else
            {
                // TODO Log "Couldn't allocate memory"
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

