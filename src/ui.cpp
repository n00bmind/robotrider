#include "imgui/imgui_draw.cpp"
#include "imgui/imgui.cpp"
// TODO Remove!
#include "imgui/imgui_demo.cpp"


// Some global constants for colors, styling, etc.
ImVec4 UInormalTextColor( 0.9f, 0.9f, 0.9f, 1.0f );
ImVec4 UItoolWindowBgColor( 0.f, 0.f, 0.f, 0.6f );

void
DEBUGDrawStats( u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImVec2 statsPos( 0, 0 );
    ImVec4 statsTextColor( .0f, .0f, .0f, 1.0f );
    ImVec4 statsBgColor( 0.5f, 0.5f, 0.5f, 0.05f );

    ImGui::SetNextWindowPos( statsPos, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, statsBgColor );
    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( statsTextColor, statsText );
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}


