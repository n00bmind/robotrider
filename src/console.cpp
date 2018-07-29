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
 
#define COMMAND(name) bool name( const char *args, char *out )
typedef COMMAND(CommandParserFunc);
struct ConsoleCommand
{
    const char *name;
    CommandParserFunc *parser;
};

internal COMMAND(CmdSub);

internal ConsoleCommand knownCommands[] =
{
    { "sub", CmdSub },
};


/////


internal void
ConsoleAddInput( GameConsole *console, ConsoleEntryType entryType, const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vsnprintf( console->entries[console->nextEntryIndex].text, CONSOLE_LINE_MAXLEN, fmt, args );
    va_end( args );

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
ConsoleExec( GameConsole *console, char *input )
{
    ConsoleAddInput( console, ConsoleEntryType::History, input );

    // Trim beginning of string
    char *cmd = input;
    while( *cmd == ' ' )
        cmd++;

    // Extract command
    const char *argString = "";
    cmd = strchr( input, ' ' );
    if( cmd )
    {
        *cmd = '\0';
        argString = cmd + 1;
    }
    cmd = input;

    // Search in known commands array
    bool found = false;
    for( u32 i = 0; i < ARRAYCOUNT(knownCommands); ++i )
    {
        if( strcmp( cmd, knownCommands[i].name ) == 0 )
        {
            char outputString[CONSOLE_LINE_MAXLEN];

            bool result = knownCommands[i].parser( argString, outputString );
            ConsoleAddInput( console, ConsoleEntryType::CommandOutput, outputString );

            found = true;
            break;
        }
    }

    if( !found )
        ConsoleAddInput( console, ConsoleEntryType::CommandOutput, "Unknown command '%s'", input );
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
        if( entryIndex >= ARRAYCOUNT(console->entries) )
            entryIndex = 0;
    }

    if( console->scrollToBottom )
        ImGui::SetScrollHere();
    console->scrollToBottom = false;

    ImGui::EndChild();
    ImGui::Separator();

    // Input
    if( ImGui::InputText( "input_console", console->inputBuffer, ARRAYCOUNT(console->inputBuffer),
                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory,
                          &ConsoleInputCallback, console ) )
    {
        ConsoleExec( console, console->inputBuffer );
        //strcpy( console->inputBuffer, "" );
        console->inputBuffer[0] = '\0';
    }

    // Keep auto focus on the input box
    if( ImGui::IsItemHovered() || (ImGui::IsRootWindowOrAnyChildFocused() && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) )
        ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}


/////


internal bool
CmdSub( const char *args, char *output )
{
    int argA, argB;
    int count = sscanf( args, "%d %d", &argA, &argB );
    // Of course, this is crappy, but just an example..
    if( count == EOF || count < 2 )
    {
        snprintf( output, CONSOLE_LINE_MAXLEN, "Error parsing arguments" );
        return false;
    }

    int result = argA - argB;
    snprintf( output, CONSOLE_LINE_MAXLEN, "%d - %d = %d", argA, argB, result );
    return true;
}

