#include <windows.h>
#include <stdint.h>

#define global static
#define internal static
#define local_persistent static


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

struct Win32OffscreenBuffer
{
    BITMAPINFO bitmapInfo;
    void *memory;
    int width;
    int height;
    int bytesPerPixel;
};

global bool globalRunning;
global Win32OffscreenBuffer globalBackBuffer;

internal void
RenderWeirdGradient( Win32OffscreenBuffer buffer, int xOffset, int yOffset )
{
    int width = buffer.width;
    int height = buffer.height;
    
    int pitch = width * buffer.bytesPerPixel;
    u8 *row = (u8 *)buffer.memory;
    for( int y = 0; y < height; ++y )
    {
        u32 *pixel = (u32 *)row;
        for( int x = 0; x < width; ++x )
        {
            u8 b = (x + xOffset);
            u8 g = (y + yOffset);
            *pixel++ = ((g << 8) | b);
        }
        row += pitch;
    }
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
Win32DisplayInWindow( Win32OffscreenBuffer buffer, HDC deviceContext, RECT clientRect,
                      int x, int y, int width, int height )
{
    int windowWidth = clientRect.right - clientRect.left;
    int windowHeight = clientRect.bottom - clientRect.top;

    StretchDIBits( deviceContext,
                   /*
                   x, y, width, height,
                   x, y, width, height,
                   */
                   0, 0, buffer.width, buffer.height,
                   0, 0, windowWidth, windowHeight,
                   buffer.memory,
                   &buffer.bitmapInfo,
                   DIB_RGB_COLORS,
                   SRCCOPY );
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
            RECT clientRect;
            GetClientRect( hwnd, &clientRect );
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            Win32ResizeDIBSection( &globalBackBuffer, width, height );
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint( hwnd, &paint );
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            RECT clientRect;
            GetClientRect( hwnd, &clientRect );
            Win32DisplayInWindow( globalBackBuffer, deviceContext, clientRect, x, y, width, height );
            EndPaint( hwnd, &paint );
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
            MSG message;
            int xOffset = 0;
            int yOffset = 0;

            globalRunning = true;
            while( globalRunning )
            {
                while( PeekMessage( &message, 0, 0, 0, PM_REMOVE ) )
                {
                    if( message.message == WM_QUIT)
                    {
                        globalRunning = false;
                    }

                    TranslateMessage( &message );
                    DispatchMessage( &message );
                }

                RenderWeirdGradient( globalBackBuffer, xOffset, yOffset );
                HDC deviceContext = GetDC( window );
                RECT clientRect;
                GetClientRect( window, &clientRect );
                int windowWidth = clientRect.right - clientRect.left;
                int windowHeight = clientRect.bottom - clientRect.top;
                Win32DisplayInWindow( globalBackBuffer, deviceContext, clientRect, 0, 0, windowWidth, windowHeight );
                ReleaseDC( window, deviceContext );

                ++xOffset;
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

