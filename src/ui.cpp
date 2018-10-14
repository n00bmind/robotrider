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

inline void
DrawAlignedQuadWithBasis( const v3& origin, const v3& xAxis, r32 xLen, const v3& yAxis, r32 yLen, u32 color,
                                 RenderCommands *renderCommands )
{
    v3 p1 = origin + xAxis * 0.f  + yAxis * 0.f;
    v3 p2 = origin + xAxis * xLen + yAxis * 0.f;
    v3 p3 = origin + xAxis * xLen + yAxis * yLen;
    v3 p4 = origin + xAxis * 0.f  + yAxis * yLen;
    PushQuad( p1, p2, p3, p4, color, renderCommands );
}

void
DrawAxisGizmos( RenderCommands *renderCommands )
{
    const m4 &currentCamTransform = renderCommands->camera.mTransform;
    v3 vCamFwd = -GetRow( currentCamTransform, 2 ).xyz;
    v3 vCamX = GetRow( currentCamTransform, 0 ).xyz;
    v3 vCamY = GetRow( currentCamTransform, 1 ).xyz;

    v3 p = GetTranslation( currentCamTransform );
    v3 pCamera = Transposed( currentCamTransform ) * (-p);

    r32 aspect = (r32)renderCommands->width / renderCommands->height;
    r32 fovYHalfRads = Radians( renderCommands->camera.fovYDeg ) / 2;
    r32 z = 1.0f;
    r32 h2 = (r32)(tan( fovYHalfRads ) * z);
	r32 w2 = h2 * aspect;

    r32 margin = 0.2f;
    r32 len = 0.1f;
    r32 w = 0.005f;
    v3 origin = pCamera + z * vCamFwd - (w2-margin) * vCamX - (h2-margin) * vCamY;

    u32 color = Pack01ToRGBA( 1, 0, 0, 1 );
    DrawAlignedQuadWithBasis( origin,                       V3X, len,  V3Z, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Z * w,             V3X, len,  V3Y, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Z * w + V3Y * w,   V3X, len, -V3Z, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Y * w,             V3X, len, -V3Y, w, color, renderCommands );

    color = Pack01ToRGBA( 0, 1, 0, 1 );
    DrawAlignedQuadWithBasis( origin,                       V3Y, len,  V3Z, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Z * w,             V3Y, len, -V3X, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Z * w - V3X * w,   V3Y, len, -V3Z, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin - V3X * w,             V3Y, len,  V3X, w, color, renderCommands );

    color = Pack01ToRGBA( 0, 0, 1, 1 );
    DrawAlignedQuadWithBasis( origin,                       V3Z, len, -V3X, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin - V3X * w,             V3Z, len,  V3Y, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin - V3X * w + V3Y * w,   V3Z, len,  V3X, w, color, renderCommands );
    DrawAlignedQuadWithBasis( origin + V3Y * w,             V3Z, len, -V3Y, w, color, renderCommands );
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
