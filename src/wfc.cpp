
internal void
PalettizeSource( const Texture& source, WFCState* state, MemoryArena* arena )
{
    state->input = PUSH_ARRAY( arena, source.width * source.height, u8 );
    state->inputWidth = source.width;
    state->inputHeight = source.height;
    state->inputChannels = source.channelCount;

    for( int i = 0; i < source.width * source.height; ++i )
    {
        bool found = false;

        u32 color = *((u32*)source.imageBuffer + i);

        // NOTE Could be ordered for speed
        for( u32 c = 0; c < state->paletteEntries; ++c )
        {
            if( state->palette[c] == color )
            {
                *(state->input + i) = (u8)c;
                found = true;
                break;
            }
        }

        if( !found )
        {
            ASSERT( state->paletteEntries < ARRAYCOUNT(state->palette) );
            state->palette[state->paletteEntries] = color;
            *(state->input + i) = (u8)state->paletteEntries;
            ++state->paletteEntries;
        }
    }
}

internal void
ExtractPattern( u8* input, u32 x, u32 y, u32 pitch, u32 N, u8* output )
{
    u8* src = input + y * pitch + x;
    u8* dst = output;
    for( u32 j = 0; j < N; ++j )
    {
        for( u32 i = 0; i < N; ++i )
        {
            *dst++ = *src++;
        }
        src += pitch - N;
    }
}

internal void
RotatePattern( u8* input, u32 N, u8* output )
{
    u8* dst = output;
    for( u32 j = 0; j < N; ++j )
    {
        for( u32 i = 0; i < N; ++i )
        {
            *dst++ = input[N - 1 - j + i * N];
        }
    }
}

internal void
ReflectPattern( u8* input, u32 N, u8* output )
{
    u8* dst = output;
    for( u32 j = 0; j < N; ++j )
    {
        for( u32 i = 0; i < N; ++i )
        {
            *dst++ = input[N - 1 - i + j * N];
        }
    }
}

inline u32
PatternHash( const WFCPattern& key, u32 entryCount )
{
    // TODO Test!
    u32 result = Fletcher32( key.data, key.width * key.width );
    return result;
}

inline bool operator ==( const WFCPattern& a, const WFCPattern& b )
{
    bool result = false;
    if( a.width == b.width )
    {
        u32 i;
        u32 len = a.width * a.width;
        for( i = 0; i < len; ++i )
            if( a.data[i] != b.data[i] )
                break;
        if( i == len )
            result = true;
    }

    return result;
}

internal void
AddPatternWithData( u8* data, u32 N, WFCState* state, MemoryArena* arena )
{
    WFCPattern testPattern = { data, N };

    u32* patternCount = state->patternsHash.Find( testPattern );
    if( patternCount )
        ++(*patternCount);
    else
    {
        u8* patternData = PUSH_ARRAY( arena, N * N, u8 );
        COPY( data, patternData, N * N );
        state->patternsHash.Add( { patternData, N }, 1, arena );
    }
}

internal void
BuildPatternsFromSource( WFCState* state, u32 N, MemoryArena* arena )
{
    u8* patternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* rotatedPatternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* reflectedPatternData = PUSH_ARRAY( arena, N * N, u8 );

    // Totally arbitrary number for the entry count
    new (&state->patternsHash) HashTable<WFCPattern, u32, PatternHash>( arena, state->inputWidth * state->inputHeight / 2 );

    for( u32 j = 0; j < state->inputHeight - N + 1; ++j )
    {
        for( u32 i = 0; i < state->inputWidth - N + 1; ++i )
        {
            ExtractPattern( state->input, i, j, state->inputWidth, N, patternData );
            AddPatternWithData( patternData, N, state, arena );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, state, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, state, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, state, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, state, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, state, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, state, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, state, arena );
        }
    }

    // TODO Pack patterns
    state->patterns = state->patternsHash.Keys( arena );
}

internal Texture
CreatePatternTexture( const WFCPattern& pattern, const u32* palette, MemoryArena* arena )
{
    u32 len = pattern.width * pattern.width;
    u32* imageBuffer = PUSH_ARRAY( arena, len, u32 );
    for( u32 i = 0; i < len; ++i )
        imageBuffer[i] = palette[pattern.data[i]];

    Texture result;
    result.handle = globalPlatform.AllocateTexture( (u8*)imageBuffer, pattern.width, pattern.width, false );
    result.imageBuffer = (u8*)imageBuffer;
    result.width = pattern.width;
    result.height = pattern.width;
    result.channelCount = 4;

    return result;
}

internal bool
Matches( u8* basePattern, v2i offset, u8* testPattern, u32 N )
{
    bool result = false;
    
    return result;
}

internal void
BuildPatternsIndex( WFCState* state, u32 N, MemoryArena* arena )
{
    u32 stride = 2 * (N - 1) + 1;
    state->indexOffsetsCount = stride * stride;
    u32 patternCount = state->patterns.count;
    new (&state->patternsIndex) Array<Array<bool>>( arena, patternCount );

    for( u32 p = 0; p < patternCount; ++p )
    {
        u8* pData = state->patterns[p].data;

        int n1 = (int)(N - 1);
        for( int j = -n1; j <= n1; ++j )
        {
            for( int i = -n1; i <= n1; ++i )
            {
                Array<bool> index( arena, patternCount );

                for( u32 q = 0; q < patternCount; ++q )
                {
                    u8* qData = state->patterns[q].data;
                    //index.Add( Matches( pData, { i, j }, qData, N ) );
                }
            }
        }
        //state->patternsIndex.Add( index );
    }
}

internal void
WFCObserve()
{

}

internal void
WFCPropagate()
{

}

void
WFCUpdate( const Texture& source, WFCState* state, MemoryArena* arena )
{
    const u32 N = 2;

    // TODO Investigate how to create temporary arenas that last more than a frame for this kind of thing
    if( !state->paletteEntries )
        PalettizeSource( source, state, arena );
    else if( state->patternsHash.count == 0 )
        BuildPatternsFromSource( state, N, arena );
    else if( !state->patternsIndex )
        BuildPatternsIndex( state, N, arena );
    else
    {
        if( !state->done )
        {
            WFCObserve();
            WFCPropagate();
        }
    }
}

void
WFCDrawTest( const EditorState& editorState, u16 screenWidth, u16 screenHeight, WFCState* state, MemoryArena* arena,
             MemoryArena* tmpArena, RenderCommands* renderCommands )
{

    ImGui::SetNextWindowPos( ImVec2( 100.f, 100.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( screenWidth * 0.75f, screenHeight * 0.75f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );
    ImGui::Begin( "window_WFC", NULL,
                  //ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::BeginChild( "child_wfc_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
    // TODO Draw indexed & de-indexed image to help detect errors
    const Texture& srcTex = editorState.testSourceTexture;
    ImGui::Image( srcTex.handle, ImVec2( (r32)srcTex.width * 8, (r32)srcTex.height * 8 ) );

    ImGui::Dummy( ImVec2( 0, 100 ) );
    ImGui::Separator();
    ImGui::Dummy( ImVec2( 0, 100 ) );

    ImGui::BeginGroup();
    if( ImGui::Button( "Patterns" ) )
        state->currentDrawnPage = WFCTestPage::Patterns;
    if( ImGui::Button( "Index" ) )
        state->currentDrawnPage = WFCTestPage::Index;
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::SameLine();

    // TODO Add a view with all extracted patterns
    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( state->currentDrawnPage )
    {
        case WFCTestPage::Patterns:
        {
            const r32 patternScale = 16;
            u32 side = (u32)Round( Sqrt( (r32)state->patterns.count ) );

            u32 row = 0;
            for( u32 i = 0; i < state->patterns.count; ++i )
            {
                WFCPattern& p = state->patterns[i];

                if( !p.texture.handle )
                    p.texture = CreatePatternTexture( p, state->palette, arena );

                ImGui::Image( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) );

                row++;
                if( row < side )
                    ImGui::SameLine( 0, 30 );
                else
                {
                    ImGui::Spacing();
                    row = 0;
                }
            }
        } break;

        case WFCTestPage::Index:
        {
            ImGui::Text( "Index world" );
        } break;
    }
    ImGui::EndChild();

    ImGui::End();
}

