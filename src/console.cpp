 
internal void
ConsoleAddInput( GameConsole *console, ConsoleEntryType entryType, const char *input )
{
    strcpy( console->entries[console->nextEntryIndex].text, input );
    console->entries[console->nextEntryIndex].type = entryType;

    if( console->entryCount < ARRAYCOUNT(console->entries) )
        console->entryCount++;

    console->nextEntryIndex++;
    if( console->nextEntryIndex >= ARRAYCOUNT(console->entries) )
        console->nextEntryIndex = 0;

    console->scrollToBottom = true;
}

void
ConsoleLog( GameConsole *console, const char *input )
{
    ConsoleAddInput( console, ConsoleEntryType::LogOutput, input );
}

void
ConsoleExec( GameConsole *console, const char *input )
{
    ConsoleAddInput( console, ConsoleEntryType::History, input );
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

void
DrawConsole( GameConsole *console, u16 windowWidth, u16 windowHeight, const char *statsText )
{
    ImGui::SetNextWindowPos( ImVec2( 0.f, 0.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( windowWidth, windowHeight * 0.25f ), ImGuiCond_Appearing );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, 100 ), ImVec2( -1, FLT_MAX ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 3.f );
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

    // Draw items
    // TODO This should be optimized to only draw items which are currently visible instead of the whole contents
    // This function can probably help (although not too sure how it works)
    //ImGui::CalcListClipping( ARRAYCOUNT(console->entries), ImGui::GetTextLineHeightWithSpacing(), &firstItemIndex, &visibleItemCount );

    u32 visibleItemCount = (u32)(ImGui::GetContentRegionAvail().y / ImGui::GetTextLineHeightWithSpacing());
    // Calc how many items to draw (always draw at least the visible number of items)
    u32 itemCount = console->entryCount;
    if( itemCount < visibleItemCount )
        itemCount = visibleItemCount;
    // Find out what's the first line to draw (discard previous values since it's always reported as 0)
    i32 firstItemIndex = console->nextEntryIndex - itemCount;
    if( firstItemIndex < 0 )
        firstItemIndex += ARRAYCOUNT(console->entries);

    u32 entryIndex = (u32)firstItemIndex;
    for( u32 i = 0; i < itemCount; ++i )
    {
        auto& entry = console->entries[entryIndex];
        const char *text = (entry.type == ConsoleEntryType::Empty) ? "\n" : entry.text;

        ImGui::PushStyleColor( ImGuiCol_Text, UInormalTextColor );
        ImGui::TextUnformatted( text, text + strlen( text ) );
        ImGui::PopStyleColor();        
        
        entryIndex++;
        if( entryIndex >= ARRAYSIZE(console->entries) )
            entryIndex = 0;
    }

    if( console->scrollToBottom )
        ImGui::SetScrollHere();
    console->scrollToBottom = false;

    ImGui::EndChild();
    ImGui::Separator();

    // Input
    if( ImGui::InputText( "input_console", console->inputBuffer, ARRAYSIZE(console->inputBuffer),
                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                          &ConsoleInputCallback, console ) )
    {
        ConsoleExec( console, console->inputBuffer );
    }

    // Keep auto focus on the input box
    if( ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) )
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

}
