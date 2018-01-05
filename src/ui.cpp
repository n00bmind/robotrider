#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
// TODO Remove!
#include "imgui/imgui_demo.cpp"


// Some global constants for colors, styling, etc.
ImVec4 UInormalTextColor( 0.9f, 0.9f, 0.9f, 1.0f );
ImVec4 UItoolWindowBgColor( 0.f, 0.f, 0.f, 0.6f );


internal bool
UIAnyMouseButtonDown()
{
    bool result = false;
    auto& io = ImGui::GetIO();

    for( int i = 0; i < ARRAYSIZE(io.MouseDown); ++i )
    {
        if( io.MouseDown[i] )
        {
            result = true;
            break;
        }
    }

    return result;
}
