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

void DrawPerformanceCounters( const DebugState* debugState, MemoryArena* tmpArena )
{
    f32 windowHeight = ImGui::GetWindowHeight();
    f32 contentWidth = ImGui::GetWindowContentRegionWidth();

    Array<DebugCounterLog> counterLogs( (DebugCounterLog*)debugState->counterLogs, (u32)ARRAYCOUNT(debugState->counterLogs) );
    counterLogs.Resize( debugState->counterLogsCount );

    int snapshotIndex = debugState->counterSnapshotIndex;
    Array<KeyIndex64> counterFrameKeys( tmpArena, counterLogs.count, Temporary() );
    BuildSortableKeysArray( counterLogs, OFFSETOF(DebugCounterLog, snapshots[snapshotIndex].frameCycles),
                            &counterFrameKeys );
    // TODO Use RadixSort11 if this gets big
    RadixSort( &counterFrameKeys, RadixKey::U64, false, tmpArena );

    f32 col1Width = contentWidth * 0.4f;
    f32 col2Width = contentWidth * 0.1f;
    f32 col3Width = contentWidth * 0.1f;
    f32 col4Width = contentWidth * 0.25f;
    f32 col5Width = contentWidth * 0.15f;

    ImGui::Columns( 5, nullptr, true );
    ImGui::SetColumnWidth( -1, col1Width );
    ImGui::Text( "Zone" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col2Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col2Width, 5, "Millis" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col3Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col3Width, 5, "Hits" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col4Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col4Width, 5, "Cycles" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col5Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col5Width, 5, "Cycles per hit" );
    ImGui::NextColumn();

    // TODO Counter stats
    // TODO Nested hierarchy
    for( int i = 0; i < counterFrameKeys.count; ++i )
    {
        DebugCounterLog const& log = counterLogs[counterFrameKeys[i].index];
        DebugCounterSnapshot const& snapshot = log.snapshots[snapshotIndex];

        u32 hitCount = snapshot.frameHits;
        u64 totalCycles = snapshot.frameCycles;
        f32 totalMillis = snapshot.frameMillis;

        if( hitCount > 0 )
        {
            // Distribute according to child region size
            ImGui::Text( "%s", log.name );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col2Width, 5, "%0.3f", totalMillis );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col3Width, 5, "%u", hitCount );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col4Width, 5, "%llu", totalCycles );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col5Width, 5, "%u", (u32)(totalCycles/hitCount) );
            ImGui::NextColumn();
        }
    }
}

void DrawPerformanceCountersTotals( const DebugState* debugState, MemoryArena* tmpArena )
{
    f32 contentWidth = ImGui::GetWindowContentRegionWidth();

    Array<DebugCounterLog> counterLogs( (DebugCounterLog*)debugState->counterLogs, (u32)ARRAYCOUNT(debugState->counterLogs) );
    counterLogs.Resize( debugState->counterLogsCount );

    Array<KeyIndex64> counterTotalKeys( tmpArena, counterLogs.count, Temporary() );
    BuildSortableKeysArray( counterLogs, OFFSETOF(DebugCounterLog, totalCycles), &counterTotalKeys );
    RadixSort( &counterTotalKeys, RadixKey::U64, false, tmpArena );

    f32 col1Width = contentWidth * 0.4f;
    f32 col2Width = contentWidth * 0.1f;
    f32 col3Width = contentWidth * 0.1f;
    f32 col4Width = contentWidth * 0.25f;
    f32 col5Width = contentWidth * 0.15f;

    ImGui::Columns( 5, nullptr, true );
    ImGui::SetColumnWidth( -1, col1Width );
    ImGui::Text( "Zone" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col2Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col2Width, 5, "Millis (est.)" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col3Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col3Width, 5, "Hits" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col4Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col4Width, 5, "Cycles" );
    ImGui::NextColumn();
    ImGui::SetColumnWidth( -1, col5Width );
    DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col5Width, 5, "Cycles per hit" );
    ImGui::NextColumn();

    for( int i = 0; i < counterTotalKeys.count; ++i )
    {
        const DebugCounterLog &log = debugState->counterLogs[counterTotalKeys[i].index];

        if( log.totalHits > 0 )
        {
            ImGui::Text( "%s", log.name );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col2Width, 5, "%0.3f", log.totalCycles / debugState->avgCyclesPerMillisecond );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col3Width, 5, "%u", log.totalHits );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col4Width, 5, "%llu", log.totalCycles );
            ImGui::NextColumn();

            DrawTextRightAligned( ImGui::GetColumnOffset( -1 ) + col5Width, 5, "%llu", (u32)(log.totalCycles / log.totalHits) );
            ImGui::NextColumn();

        }
    }
}

void DrawPerformanceCountersWindow( const DebugState* debugState, u32 windowWidth, u32 windowHeight, MemoryArena* tmpArena )
{
    ImGui::SetNextWindowPos( ImVec2( 0.f, windowHeight * 0.25f ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( (f32)windowWidth, windowHeight * 0.5f ), ImGuiCond_Always );
    //ImGui::SetNextWindowSizeConstraints( ImVec2( -1, 100 ), ImVec2( -1, FLT_MAX ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.f );
    //ImGui::PushStyleColor( ImGuiCol_WindowBg, UItoolWindowBgColor );
    //ImGui::PushStyleColor( ImGuiCol_Text, UInormalTextColor );

    ImGui::Begin( "window_middle_panel", NULL,
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

    ImGui::BeginChild( "child_middle_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.5f, -50.f ), true );
    DrawPerformanceCounters( debugState, tmpArena );
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild( "child_middle_right", ImVec2( 0.f, -50.f ), true );
    DrawPerformanceCountersTotals( debugState, tmpArena );
    ImGui::EndChild();

    // Buttons row
    ImGui::BeginChild( "child_buttons_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.5f, 0.f ), false );
    ImGui::Button( "Counters" );
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild( "child_buttons_right", ImVec2( 0.f, 0.f ), false );
    ImGui::Button( "Totals" );
    ImGui::EndChild();



    ImGui::End();
    //ImGui::PopStyleColor();        
    //ImGui::PopStyleColor();
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

