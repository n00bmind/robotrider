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

// Some global constants for colors, styling, etc.
ImVec4 UInormalTextColor( 0.9f, 0.9f, 0.9f, 1.0f );
ImVec4 UIdarkTextColor( .0f, .0f, .0f, 1.0f );
ImVec4 UItoolWindowBgColor( 0.f, 0.f, 0.f, 0.6f );
ImVec4 UIlightOverlayBgColor( 0.5f, 0.5f, 0.5f, 0.05f );

void
DrawStats( u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImVec2 statsPos( 0, 0 );

    ImGui::SetNextWindowPos( statsPos, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UIlightOverlayBgColor );
    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( UIdarkTextColor, statsText );
#if !RELEASE
    ImGui::SameLine( (r32)windowWidth - 100 );
    ImGui::TextColored( UIdarkTextColor, "DEBUG BUILD" );
#endif
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
DrawEditorStats( u16 windowWidth, u16 windowHeight, const char* statsText, bool blinkToggle )
{
    ImVec2 statsPos( 0, 0 );

    ImGui::SetNextWindowPos( statsPos, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UIlightOverlayBgColor );
    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( UIdarkTextColor, statsText );
    ImGui::SameLine( (r32)windowWidth - 100 );
    ImGui::TextColored( UIdarkTextColor, blinkToggle ? "EDITOR MODE" : "" );
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
DrawAxisGizmos( RenderCommands *renderCommands )
{
    // FIXME Draw 4 quads per axis so they are always visible from any angle!

    const m4 &currentCamTransform = renderCommands->camera.mTransform;
    v3 vCamFwd = -GetRow( currentCamTransform, 2 ).xyz;
    v3 vCamX = GetRow( currentCamTransform, 0 ).xyz;
    v3 vCamY = GetRow( currentCamTransform, 1 ).xyz;

    v3 p = GetTranslation( currentCamTransform );
    v3 pCamera = Transposed( currentCamTransform ) * (-p);

    float aspect = (r32)renderCommands->width / renderCommands->height;
    float fovYHalfRads = Radians( renderCommands->camera.fovYDeg ) / 2;
    float z = 1.0f;
    float h2 = (r32)(tan( fovYHalfRads ) * z);
	float w2 = h2 * aspect;

    float margin = 0.2f;
    float len = 0.1f;
    float thick = 0.005f;
    v3 startPos = pCamera + z * vCamFwd - (w2-margin) * vCamX - (h2-margin) * vCamY;

    v3 p1, p2, p3, p4;
    u32 color = Pack01ToRGBA( V4( 1, 0, 0, 1 ) );
    v3 vL = V3X();
    v3 vD = V3Z();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );
    vD = V3Y();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );

    vL = V3Y();
    vD = V3X();
    color = Pack01ToRGBA( V4( 0, 1, 0, 1 ) );
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );
    vD = V3Z();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );

    vL = V3Z();
    vD = V3X();
    color = Pack01ToRGBA( V4( 0, 0, 1, 1 ) );
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );
    vD = V3Y();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( p1, p2, p3, p4, color, renderCommands );
}

#if !RELEASE
void
DrawPerformanceCounters( const GameMemory* gameMemory, u32 windowWidth, u32 windowHeight )
{
    ImGui::SetNextWindowPos( ImVec2( 100.f, windowHeight * 0.25f + 100 ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 500.f, windowHeight * 0.25f ), ImGuiCond_Appearing );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, 100 ), ImVec2( -1, FLT_MAX ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 3.f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UItoolWindowBgColor );
    ImGui::Begin( "window_performance_counters", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::PushStyleColor( ImGuiCol_Text, UInormalTextColor );

    // TODO Counter stats
    DebugState* debugState = (DebugState*)gameMemory->debugStorage;
    for( u32 i = 0; i < debugState->counterLogsCount; ++i )
    {
        DebugCounterLog &log = debugState->counterLogs[i];

        u32 frameHitCount = log.snapshots[0].hitCount;
        u64 frameCycleCount = log.snapshots[0].cycleCount;

        if( frameHitCount > 0 )
        {
            ImGui::Text( "%s@%u\t%llu fc  %u h  %u fc/h",
                         log.function,
                         log.lineNumber,
                         frameCycleCount,
                         frameHitCount,
                         (u32)(frameCycleCount/frameHitCount) );
        }
    }
    ImGui::PopStyleColor();        

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
#endif
