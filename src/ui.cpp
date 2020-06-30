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

#if NON_UNITY_BUILD
#include "ui.h"
#include "util.h"
#include "renderer.h"
#include "debugstats.h"
#include "editor.h"
#endif

// TODO Make sure everything here uses resolution-independant (0 to 1) coordinates


void DrawStats( u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImVec2 statsPos( 0, windowHeight );
    ImGui::SetNextWindowPos( statsPos, ImGuiCond_Always, ImVec2( 0, 1.f ) );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UIlightOverlayBgColor );

    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( UIdarkTextColor, statsText );
#if !RELEASE
    ImGui::SameLine( (f32)windowWidth - 100 );
    ImGui::TextColored( UIdarkTextColor, "DEBUG BUILD" );
#endif
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void DrawEditorStats( u16 windowWidth, u16 windowHeight, const char* statsText, bool blinkToggle )
{
    ImVec2 statsPos( 0, windowHeight );
    ImGui::SetNextWindowPos( statsPos, ImGuiCond_Always, ImVec2( 0, 1.f ) );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, ImGui::GetTextLineHeight() ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.0f );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.0f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UIlightOverlayBgColor );

    ImGui::Begin( "window_stats", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove |
                  ImGuiWindowFlags_NoInputs );
    ImGui::TextColored( UIdarkTextColor, statsText );
    ImGui::SameLine( (f32)windowWidth - 100 );
    ImGui::TextColored( UIdarkTextColor, blinkToggle ? "EDITOR MODE" : "" );
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

inline void DrawAlignedQuadWithBasis( const v3& origin, const v3& xAxis, f32 xLen, const v3& yAxis, f32 yLen, u32 color,
                                      RenderCommands *renderCommands )
{
    v3 p1 = origin + xAxis * 0.f  + yAxis * 0.f;
    v3 p2 = origin + xAxis * xLen + yAxis * 0.f;
    v3 p3 = origin + xAxis * xLen + yAxis * yLen;
    v3 p4 = origin + xAxis * 0.f  + yAxis * yLen;
    RenderQuad( p1, p2, p3, p4, color, renderCommands );
}

void DrawAxisGizmos( RenderCommands *renderCommands )
{
    const m4 &currentCamTransform = renderCommands->camera.worldToCamera;
    v3 vCamX = GetCameraBasisX( currentCamTransform );
    v3 vCamY = GetCameraBasisY( currentCamTransform );
    v3 vCamFwd = -GetCameraBasisZ( currentCamTransform );

    v3 p = GetTranslation( currentCamTransform );
    v3 pCamera = Transposed( currentCamTransform ) * (-p);

    f32 aspect = (f32)renderCommands->width / renderCommands->height;
    f32 fovYHalfRads = Radians( renderCommands->camera.fovYDeg ) / 2;
    f32 z = 1.0f;
    f32 h2 = (f32)(tan( fovYHalfRads ) * z);
	f32 w2 = h2 * aspect;

    f32 margin = 0.2f;
    f32 len = 0.1f;
    f32 w = 0.005f;
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

void DrawTextRightAligned( f32 cursorStartX, f32 rightPadding, const char* format, ... )
{
    char textBuffer[1024];

    va_list args;
    va_start( args, format );
    vsnprintf( textBuffer, ARRAYCOUNT(textBuffer), format, args );
    va_end( args );

    ImVec2 textSize = ImGui::CalcTextSize( textBuffer );
    ImGui::SetCursorPosX( cursorStartX - textSize.x - rightPadding );
    ImGui::Text( textBuffer );
}

void DrawPerformanceCounters( const DebugState* debugState, const TemporaryMemory& tmpMemory )
{
    f32 windowHeight = ImGui::GetWindowHeight();

    ImGui::BeginChild( "child_perf_counters_frame", ImVec2( -16, windowHeight / 2 ) );
    f32 contentWidth = ImGui::GetWindowWidth();
    ImGui::Columns( 4, nullptr, true );

    Array<DebugCounterLog> counterLogs( (DebugCounterLog*)debugState->counterLogs,
                                                                 (u32)ARRAYCOUNT(debugState->counterLogs) );
    counterLogs.count = debugState->counterLogsCount;

    int snapshotIndex = debugState->counterSnapshotIndex;
    Array<KeyIndex64> counterFrameKeys( tmpMemory.arena, counterLogs.count, Temporary() );
    BuildSortableKeysArray( counterLogs, OFFSETOF(DebugCounterLog, snapshots[snapshotIndex].cycleCount),
                            &counterFrameKeys );
    // TODO Use RadixSort11 if this gets big
    RadixSort( &counterFrameKeys, RadixKey::U64, false, tmpMemory.arena );

    // TODO Counter stats
    for( int i = 0; i < counterFrameKeys.count; ++i )
    {
        const DebugCounterLog &log = counterLogs[counterFrameKeys[i].index];

        u32 frameHitCount = log.snapshots[snapshotIndex].hitCount;
        u64 frameCycleCount = log.snapshots[snapshotIndex].cycleCount;

        if( frameHitCount > 0 )
        {
            // Distribute according to child region size
            f32 currentWidth = contentWidth * 0.45f;
            ImGui::SetColumnWidth( -1, currentWidth );
            ImGui::Text( "%s @ %u", log.function, log.lineNumber );
            ImGui::NextColumn();

            currentWidth = contentWidth * 0.2f;
            ImGui::SetColumnWidth( -1, currentWidth );
            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + currentWidth, 5, "%llu fc", frameCycleCount );
            ImGui::NextColumn();

            currentWidth = contentWidth * 0.15f;
            ImGui::SetColumnWidth( -1, currentWidth );
            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + currentWidth, 5, "%u h", frameHitCount );
            ImGui::NextColumn();

            currentWidth = contentWidth * 0.2f;
            ImGui::SetColumnWidth( -1, currentWidth );
            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + currentWidth, 5, "%u fc/h", (u32)(frameCycleCount/frameHitCount) );
            ImGui::NextColumn();
        }
    }
    ImGui::EndChild();

    // Span vertically for now
    //ImGui::Dummy( V2( 0, 250 ) );

    ImGui::BeginChild( "child_perf_counters_total", ImVec2( -16, -16 ) );
    contentWidth = ImGui::GetWindowWidth();
    ImGui::Columns( 3, nullptr, true );

    Array<KeyIndex64> counterTotalKeys( tmpMemory.arena, counterLogs.count, Temporary() );
    BuildSortableKeysArray( counterLogs, OFFSETOF(DebugCounterLog, totalCycleCount), &counterTotalKeys );
    RadixSort( &counterTotalKeys, RadixKey::U64, false, tmpMemory.arena );

    for( int i = 0; i < counterTotalKeys.count; ++i )
    {
        const DebugCounterLog &log = debugState->counterLogs[counterTotalKeys[i].index];

        if( log.totalHitCount > 0 )
        {
            f32 currentWidth = contentWidth * 0.5f;
            ImGui::SetColumnWidth( -1, currentWidth );
            ImGui::Text( "%s @ %u", log.function, log.lineNumber );
            ImGui::NextColumn();

            currentWidth = contentWidth * 0.25f;
            ImGui::SetColumnWidth( -1, currentWidth );
            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + currentWidth, 5, "%llu tc", log.totalCycleCount );
            ImGui::NextColumn();

            currentWidth = contentWidth * 0.25f;
            ImGui::SetColumnWidth( -1, currentWidth );
            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + currentWidth, 5, "%u th", log.totalHitCount );
            ImGui::NextColumn();
        }
    }
    ImGui::EndChild();
}

void DrawPerformanceCountersWindow( const DebugState* debugState, u32 windowWidth, u32 windowHeight, const TemporaryMemory& tmpMemory )
{
    ImGui::SetNextWindowPos( ImVec2( 100.f, windowHeight * 0.25f + 100 ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( 500.f, windowHeight * 0.25f ), ImGuiCond_Always );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, 100 ), ImVec2( -1, FLT_MAX ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 3.f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UItoolWindowBgColor );
    ImGui::Begin( "window_performance_counters", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::PushStyleColor( ImGuiCol_Text, UInormalTextColor );

    DrawPerformanceCounters( debugState, tmpMemory );
    ImGui::PopStyleColor();        

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void DrawEditorStateWindow( const v2i& windowP, const v2i& windowDim, const EditorState& state )
{
    ImGui::SetNextWindowPos( windowP, ImGuiCond_Always );
    ImGui::SetNextWindowSize( windowDim, ImGuiCond_Always );

    ImGui::Begin( "window_editor_state", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoMove );

    ImGui::Text( "Camera translation speed: %d", state.translationSpeedStep );
    ImGui::End();
}

