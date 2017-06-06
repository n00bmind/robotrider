#include <windows.h>


LRESULT CALLBACK WindowProc( HWND hwnd,
                             UINT  uMsg,
                             WPARAM wParam,
                             LPARAM lParam )
{
    LRESULT result = 0;
    
    switch(uMsg)
    {
        case WM_PAINT:
        {
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

    windowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.lpszClassName = "RobotRiderWindowClass";

    if( RegisterClass( &windowClass ) )
    {
        HWND windowHandle = CreateWindowEx( 0,
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
                                            
        if( windowHandle )
        {
            MSG message;
            for( ;; )
            {
                BOOL messageResult = GetMessage( &message, 0, 0, 0 );

                if( messageResult > 0 )
                {
                    TranslateMessage( &message );
                    DispatchMessage( &message );
                }
                else
                {
                    break;
                }
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
    return 0;
}

