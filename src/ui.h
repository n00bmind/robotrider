#pragma once

#if NON_UNITY_BUILD
#include "imgui/imgui.h"
#endif

// Some global constants for colors, styling, etc.
const ImVec4 UInormalTextColor( 0.9f, 0.9f, 0.9f, 1.0f );
const ImVec4 UIdarkTextColor( .0f, .0f, .0f, 1.0f );
const ImVec4 UItoolWindowBgColor( 0.f, 0.f, 0.f, 0.6f );
const ImVec4 UIlightOverlayBgColor( 0.5f, 0.5f, 0.5f, 0.1f );


struct RenderCommands;
struct DebugState;
struct EditorState;

void DrawStats( u16 windowWidth, u16 windowHeight, const char *statsText );
void DrawEditorStats( u16 windowWidth, u16 windowHeight, const char* statsText, bool blinkToggle );
void DrawAxisGizmos( RenderCommands *renderCommands );
void DrawTextRightAligned( r32 cursorStartX, r32 rightPadding, const char* format, ... );
void DrawPerformanceCounters( const DebugState* debugState, const TemporaryMemory& tmpMemory );
void DrawPerformanceCountersWindow( const DebugState* debugState, u32 windowWidth, u32 windowHeight, const TemporaryMemory& tmpMemory );
void DrawEditorStateWindow( const v2i& windowP, const v2i& windowDim, const EditorState& state );
