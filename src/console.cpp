 
internal void
ConsoleExec( const char *input )
{
    // TODO Trim input and extract command + args
    // TODO Parse existing commands from a hot-reloadable file
}

internal int
ConsoleInputCallback( ImGuiTextEditCallbackData *data )
{
    GameConsole *console = (GameConsole *)data->UserData;

    // TODO 
    switch( data->EventFlag )
    {
        case ImGuiInputTextFlags_CallbackCompletion:
        {

        } break;

        case ImGuiInputTextFlags_CallbackHistory:
        {

        } break;
    }

    return 0;
}

internal void
DrawConsole( GameConsole *console, u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImGui::SetNextWindowPos( ImVec2( 0.f, 0.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, windowHeight * 0.25f ), ImGuiCond_Appearing );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, 100 ), ImVec2( -1, FLT_MAX ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.3f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, UItoolWindowBgColor );
    ImGui::Begin( "window_console", NULL,
                  ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::TextColored( UInormalTextColor, statsText );
    ImGui::Spacing();
    ImGui::Separator();

    // Reserve space for a separator and one line of text input
    float footerHeight = ImGui::GetItemsLineHeightWithSpacing();
    ImGui:: BeginChild( "scroll_console", ImVec2( 0, -footerHeight), false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_HorizontalScrollbar );

    for( int i = 0; i < ARRAYSIZE(console->entries); ++i )
        snprintf( console->entries[i].text, ARRAYSIZE(console->entries[i].text), "This is line %d", i );

    // Draw items
    int firstItem, lastItem;
    //ImGui::CalcListClipping( ARRAYSIZE(console->entries), ImGui::GetTextLineHeightWithSpacing(), &firstItem, &lastItem );
    // TODO Draw items in a ring buffer fashion
    firstItem = 0; lastItem = ARRAYSIZE(console->entries);
    for( int l = firstItem; l < lastItem; ++l )
        ImGui::TextColored( UInormalTextColor, console->entries[l].text );

    if( console->scrollToBottom )
        ImGui::SetScrollHere();
    console->scrollToBottom = false;

    ImGui::EndChild();
    ImGui::Separator();

    // Input
    // TODO Redirect platform keys to ImGui!!
    if( ImGui::InputText( "input_console", console->inputBuffer, ARRAYSIZE(console->inputBuffer),
                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                          &ConsoleInputCallback, console ) )
    {
        console->scrollToBottom = true;
        ConsoleExec( console->inputBuffer );
    }

    // Keep auto focus on the input box
    if( ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) )
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

}
