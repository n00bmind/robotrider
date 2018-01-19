
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
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
DrawEditorNotice( u16 windowWidth, u16 windowHeight, bool blinkToggle )
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
    ImGui::TextColored( UIdarkTextColor, blinkToggle ? "EDITOR MODE" : "" );
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void
DrawAxisGizmos( GameRenderCommands *renderCommands )
{
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
    u32 color = RGBAPack( 255 * V4( 1, 0, 0, 1 ) );
    v3 vL = V3X();
    v3 vD = V3Z();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );
    vD = V3Y();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );

    vL = V3Y();
    vD = V3X();
    color = RGBAPack( 255 * V4( 0, 1, 0, 1 ) );
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );
    vD = V3Z();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );

    vL = V3Z();
    vD = V3X();
    color = RGBAPack( 255 * V4( 0, 0, 1, 1 ) );
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );
    vD = V3Y();
    p1 = startPos + vL * 0.f + vD * thick;
    p2 = startPos + vL * 0.f + vD * 0.f;
    p3 = startPos + vL * len + vD * 0.f;
    p4 = startPos + vL * len + vD * thick;
    PushQuad( renderCommands, p1, p2, p3, p4, color );
}
