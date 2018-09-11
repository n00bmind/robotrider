
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
    bool result = true;

    u32 xMin = offset.x < 0 ? 0 : offset.x, xMax = offset.x < 0 ? offset.x + N : N;
    u32 yMin = offset.y < 0 ? 0 : offset.y, yMax = offset.y < 0 ? offset.y + N : N;

    for( u32 y = yMin; y < yMax; ++y )
    {
        for( u32 x = xMin; x < xMax; ++x )
        {
            u32 x2 = x - offset.x;
            u32 y2 = y - offset.y;

            if( basePattern[y * N + x] != testPattern[y2 * N + x2] )
            {
                result = false;
                break;
            }
        }
    }

    return result;
}

internal void
BuildPatternsIndex( WFCState* state, u32 N, MemoryArena* arena )
{
    v2i offsets[4] =
    {
        { -1,  0 },
        {  0,  1 },
        {  1,  0 },
        {  0, -1 },
    };

    u32 patternCount = state->patterns.count;
    for( u32 d = 0; d < ARRAYCOUNT(state->patternsIndex); ++d )
    {
        Array<WFCIndexEntry> entries( arena, patternCount );

        for( u32 p = 0; p < patternCount; ++p )
        {
            u8* pData = state->patterns[p].data;

            Array<bool> matches( arena, patternCount );

            for( u32 q = 0; q < patternCount; ++q )
            {
                u8* qData = state->patterns[q].data;
                matches.Add( Matches( pData, offsets[d], qData, N ) );
            }
            entries.Add( { matches } );
        }
        state->patternsIndex[d] = { entries };
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
    {
        BuildPatternsFromSource( state, N, arena );
        BuildPatternsIndex( state, N, arena );
    }
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
    const r32 patternScale = 16;

    ImGui::SetNextWindowPos( ImVec2( 100.f, 100.f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( screenWidth * 0.75f, screenHeight * 0.75f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );
    ImGui::Begin( "window_WFC", NULL,
                  //ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::BeginChild( "child_wfc_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
    // TODO Draw indexed & de-indexed image to help detect errors
    const Texture& srcTex = editorState.testSourceTexture;
    ImGui::Image( srcTex.handle, ImVec2( (r32)srcTex.width * patternScale, (r32)srcTex.height * patternScale ) );

    ImGui::Dummy( ImVec2( 0, 100 ) );
    ImGui::Separator();
    ImGui::Dummy( ImVec2( 0, 20 ) );

    ImGui::BeginGroup();
    if( ImGui::Button( "Patterns" ) )
        state->currentPage = WFCTestPage::Patterns;
    if( ImGui::Button( "Index" ) )
        state->currentPage = WFCTestPage::Index;
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::SameLine();

    u32 patternsCountSqrt = (u32)Round( Sqrt( (r32)state->patterns.count ) );

    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( state->currentPage )
    {
        case WFCTestPage::Patterns:
        {
            u32 row = 0;
            for( u32 i = 0; i < state->patterns.count; ++i )
            {
                WFCPattern& p = state->patterns[i];

                if( !p.texture.handle )
                    p.texture = CreatePatternTexture( p, state->palette, arena );

                ImGui::Image( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) );

                row++;
                if( row < patternsCountSqrt )
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
            ImGui::BeginChild( "child_index_entries", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
            for( u32 i = 0; i < state->patterns.count; ++i )
            {
                WFCPattern& p = state->patterns[i];

                if( !p.texture.handle )
                    p.texture = CreatePatternTexture( p, state->palette, arena );

                if( ImGui::ImageButton( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) ) )
                    state->currentIndexEntry = i;
                ImGui::Spacing();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild( "child_index_offsets", ImVec2( 0, 0 ), false, 0 );
            ImGui::Columns( 3, nullptr, true );
            for( u32 j = 0; j < 3; ++j )
            {
                for( u32 i = 0; i < 3; ++i )
                {
                    int d = -1;

                    if( ImGui::GetColumnIndex() == 0 )
                        ImGui::Separator();
                    ImGui::Text("{ %d, %d }", i, j );

                    if( i == 0 && j == 1 )
                        d = 0;
                    else if( i == 1 && j == 2 )
                        d = 1;
                    else if( i == 2 && j == 1 )
                        d = 2;
                    else if( i == 1 && j == 0 )
                        d = 3;

                    if( d >= 0 )
                    {
                        Array<bool>& matches = state->patternsIndex[d].entries[state->currentIndexEntry].matches;

                        u32 row = 0;
                        for( u32 m = 0; m < state->patterns.count; ++m )
                        {
                            if( !matches[m] )
                                continue;

                            WFCPattern& p = state->patterns[m];

                            //if( !p.texture.handle )
                                //p.texture = CreatePatternTexture( p, state->palette, arena );

                            ImGui::Image( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) );

                            row++;
                            if( row < patternsCountSqrt )
                                ImGui::SameLine( 0, 30 );
                            else
                            {
                                ImGui::Spacing();
                                row = 0;
                            }
                        }
                    }

                    ImGui::NextColumn();
                }
            }
            ImGui::Columns( 1 );
            ImGui::Separator();
            ImGui::EndChild();
        } break;
    }
    ImGui::EndChild();

    ImGui::End();
}

