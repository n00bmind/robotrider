#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>

#define global static
#define internal static
#define local_persistent static

#define PI32 3.141592653589f

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
Win32LoadXInput()
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
                DSBUFFERDESC bufferDescription = {};
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // TODO DSBAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER primaryBuffer;
                if( SUCCEEDED(directSound->CreateSoundBuffer( &bufferDescription, &primaryBuffer, 0 )) )
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
                        DSBUFFERDESC bufferDescription = {};
                        bufferDescription.dwSize = sizeof(bufferDescription);
                        bufferDescription.dwFlags = 0;
                        bufferDescription.dwBufferBytes = bufferSize;
                        bufferDescription.lpwfxFormat = &waveFormat;

                        if( SUCCEEDED(directSound->CreateSoundBuffer( &bufferDescription, &globalSecondaryBuffer, 0 )) )
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
Win32ClearSoundBuffer( Win32SoundOutput *soundOutput )
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;

    if( SUCCEEDED(globalSecondaryBuffer->Lock( 0, soundOutput->secondaryBufferSize,
                                               &region1, &region1Size,
                                               &region2, &region2Size,
                                               0 )) )
    {
        u8 *destByte = (u8 *)region1;
        for( DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex )
        {
            *destByte++ = 0;
        }
        destByte = (u8 *)region2;
        for( DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex )
        {
            *destByte++ = 0;
        }

        globalSecondaryBuffer->Unlock( region1, region1Size, region2, region2Size );
    }
}

internal void
Win32FillSoundBuffer( Win32SoundOutput *soundOutput, DWORD byteToLock, DWORD bytesToWrite,
                      GameSoundBuffer *sourceBuffer )
{
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
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        for( DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }

        destSample = (s16 *)region2;
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        for( DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex )
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }
        globalSecondaryBuffer->Unlock( region1, region1Size, region2, region2Size );
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
Win32ResizeDIBSection( Win32OffscreenBuffer *buffer, int width, int height )
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

internal void
Win32ProcessXInputDigitalButton( DWORD buttonStateBits, GameButtonState *oldState, DWORD buttonBit,
                                 GameButtonState *newState )
{
    newState->endedDown = ((buttonStateBits & buttonBit) == buttonBit);
    newState->halfTransitionCount = (oldState->endedDown != newState->endedDown) ? 1 : 0;
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
            u32 vkCode = wParam;
            bool wasDown = ((lParam & (1 << 30)) != 0);
            bool isDown =  ((lParam & (1 << 31)) == 0);
            if( isDown != wasDown )
            {
                if( vkCode == 'W' )
                {

                }
                else if( vkCode == 'A' )
                {

                }
                else if( vkCode == 'S' )
                {

                }
                else if( vkCode == 'D' )
                {

                }
                else if( vkCode == 'Q' )
                {

                }
                else if( vkCode == 'E' )
                {

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
                }
            }

            b32 altKeyWasDown = ((lParam & (1<<29)) != 0);
            if( vkCode == VK_F4 && altKeyWasDown )
            {
                globalRunning = false;
            }
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

int CALLBACK
WinMain( HINSTANCE hInstance,
         HINSTANCE hPrevInstance,
         LPSTR lpCmdLine,
         int nCmdShow )
{
    Win32LoadXInput();
    Win32ResizeDIBSection( &globalBackBuffer, 1280, 720 );

    LARGE_INTEGER perfCounterFrequency;
    QueryPerformanceFrequency( &perfCounterFrequency );

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
            Win32SoundOutput soundOutput = {};
            soundOutput.samplesPerSecond = 48000;
            soundOutput.bytesPerSample = sizeof(s16)*2;  
            // FIXME How come a value of 375 (8ms.) produces no glitches at all while SPS / 30 (33ms.) is glitchy as hell!?
            soundOutput.latencySamples = soundOutput.samplesPerSecond / 15; // ~66ms.
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.runningSampleIndex = 0;

            Win32InitDirectSound( window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize );
            Win32ClearSoundBuffer( &soundOutput );
            globalSecondaryBuffer->Play( 0, 0, DSBPLAY_LOOPING );

            s16 *soundSamples = (s16 *)VirtualAlloc( 0, soundOutput.secondaryBufferSize,
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

                HDC deviceContext = GetDC( window );
                globalRunning = true;

                LARGE_INTEGER lastCounter;
                QueryPerformanceCounter( &lastCounter );
                s64 lastCycleCounter = __rdtsc();

                while( globalRunning )
                {
                    MSG message;

                    while( PeekMessage( &message, 0, 0, 0, PM_REMOVE ) )
                    {
                        if( message.message == WM_QUIT)
                        {
                            globalRunning = false;
                        }

                        TranslateMessage( &message );
                        DispatchMessage( &message );
                    }

                    // TODO Should we poll this more frequently?
                    u32 maxControllerCount = XUSER_MAX_COUNT;
                    if( maxControllerCount > ARRAYCOUNT( newInput->controllers ) )
                    {
                        maxControllerCount = ARRAYCOUNT( newInput->controllers );
                    }

                    for( u32 controllerIndex = 0; controllerIndex < maxControllerCount; ++controllerIndex )
                    {
                        GameControllerInput *oldController = &oldInput->controllers[controllerIndex];
                        GameControllerInput *newController = &newInput->controllers[controllerIndex];

                        XINPUT_STATE controllerState;
                        if( XInputGetState( controllerIndex, &controllerState ) == ERROR_SUCCESS )
                        {
                            // Plugged in
                            XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

                            // TODO DPad!
                            bool dPadUp = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            bool dPadDown = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            bool dPadLeft = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            bool dPadRight = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

                            r32 x;
                            if( pad->sThumbLX < 0 )
                            {
                                x = (r32)pad->sThumbLX / 32768.0f;
                            }
                            else
                            {
                                x = (r32)pad->sThumbLX / 32767.0f;
                            }
                            newController->minX = newController->maxX = newController->endX = x;

                            r32 y;
                            if( pad->sThumbLY < 0 )
                            {
                                y = (r32)pad->sThumbLY / 32768.0f;
                            }
                            else
                            {
                                y = (r32)pad->sThumbLY / 32767.0f;
                            }
                            newController->minY = newController->maxY = newController->endY = y;

                            s16 stickX = pad->sThumbLX;
                            s16 stickY = pad->sThumbLY;

                            Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->aButton,
                                                             XINPUT_GAMEPAD_A, &newController->aButton );
                            Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->bButton,
                                                             XINPUT_GAMEPAD_B, &newController->bButton );
                            Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->xButton,
                                                             XINPUT_GAMEPAD_X, &newController->xButton );
                            Win32ProcessXInputDigitalButton( pad->wButtons, &oldController->yButton,
                                                             XINPUT_GAMEPAD_Y, &newController->yButton );
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
                        }
                    }

                    DWORD byteToLock;
                    DWORD bytesToWrite;
                    DWORD targetCursor;
                    DWORD playCursor, writeCursor;

                    b32 soundIsValid = false;
                    // TODO Tighten up sound logic so that we know where we should be writing to
                    // in order to anticipate the time spent in the game update!
                    if( SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition( &playCursor, &writeCursor )) )
                    {
                        byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample)
                            % soundOutput.secondaryBufferSize;
                        targetCursor = (playCursor + (soundOutput.latencySamples * soundOutput.bytesPerSample))
                            % soundOutput.secondaryBufferSize;

                        if( byteToLock > targetCursor )
                        {
                            bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                            bytesToWrite += targetCursor;
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }

                        soundIsValid = true;
                    }

                    GameOffscreenBuffer videoBuffer = {};
                    videoBuffer.memory = globalBackBuffer.memory;
                    videoBuffer.width = globalBackBuffer.width;
                    videoBuffer.height = globalBackBuffer.height;
                    videoBuffer.bytesPerPixel = globalBackBuffer.bytesPerPixel;

                    GameSoundBuffer soundBuffer = {};
                    soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                    // TODO Check if we have crackles when going lower than 30 FPS
                    soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.samples = soundSamples;

                    GameUpdateAndRender( &gameMemory, newInput, &videoBuffer, &soundBuffer );

                    if( soundIsValid )
                    {
                        Win32FillSoundBuffer( &soundOutput, byteToLock, bytesToWrite, &soundBuffer );
                    }

                    Win32WindowDimension dim = Win32GetWindowDimension( window );
                    Win32DisplayInWindow( &globalBackBuffer, deviceContext, dim.width, dim.height );

                    GameInput *temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;

                    LARGE_INTEGER endCounter;
                    QueryPerformanceCounter( &endCounter );
                    s64 endCycleCounter = __rdtsc();

                    u64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
                    u64 cyclesElapsed = endCycleCounter - lastCycleCounter;

                    r32 msElapsed = 1000.f * counterElapsed / perfCounterFrequency.QuadPart;
                    r32 fps = (r32)perfCounterFrequency.QuadPart / counterElapsed;
                    u32 kCyclesElapsed = (u32)(cyclesElapsed / 1000);
#if 0
                    char buffer[256];
                    sprintf( buffer, "FPS: %.02f (%d Kcycles)\n", fps, kCyclesElapsed );
                    OutputDebugString( buffer );
#endif

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

