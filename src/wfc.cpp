namespace WFC
{


internal void
PalettizeSource( const Texture& source, State* state, MemoryArena* arena )
{
    state->input = PUSH_ARRAY( arena, source.width * source.height, u8 );

    for( int i = 0; i < source.width * source.height; ++i )
    {
        u32 color = *((u32*)source.imageBuffer + i);

        bool found = false;
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
PatternHash( const Pattern& key, u32 entryCount )
{
    // TODO Test!
    u32 result = Fletcher32( key.data, key.stride * key.stride );
    return result;
}

inline bool operator ==( const Pattern& a, const Pattern& b )
{
    bool result = false;
    if( a.stride == b.stride )
    {
        u32 i;
        u32 len = a.stride * a.stride;
        for( i = 0; i < len; ++i )
            if( a.data[i] != b.data[i] )
                break;
        if( i == len )
            result = true;
    }

    return result;
}

internal void
AddPatternWithData( u8* data, const u32 N, State* state, MemoryArena* arena )
{
    Pattern testPattern = { data, N };

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
BuildPatternsFromSource( const u32 N, const v2i& inputDim, State* state, MemoryArena* arena )
{
    u8* patternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* rotatedPatternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* reflectedPatternData = PUSH_ARRAY( arena, N * N, u8 );

    // Totally arbitrary number for the entry count
    new (&state->patternsHash) HashTable<Pattern, u32, PatternHash>( arena, inputDim.x * inputDim.y / 2 );

    for( u32 j = 0; j < inputDim.y - N + 1; ++j )
    {
        for( u32 i = 0; i < inputDim.x - N + 1; ++i )
        {
            ExtractPattern( state->input, i, j, inputDim.x, N, patternData );
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
CreatePatternTexture( const Pattern& pattern, const u32* palette, MemoryArena* arena )
{
    u32 len = pattern.stride * pattern.stride;
    u32* imageBuffer = PUSH_ARRAY( arena, len, u32 );
    for( u32 i = 0; i < len; ++i )
        imageBuffer[i] = palette[pattern.data[i]];

    Texture result;
    result.handle = globalPlatform.AllocateTexture( (u8*)imageBuffer, pattern.stride, pattern.stride, false, nullptr );
    result.imageBuffer = (u8*)imageBuffer;
    result.width = pattern.stride;
    result.height = pattern.stride;
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

// TODO Is this order relevant?
internal const v2i cellDeltas[Adjacency::Count] =
{
    { -1,  0 },     // Left
    {  0,  1 },     // Bottom
    {  1,  0 },     // Right
    {  0, -1 },     // Top
};

internal const u32 opposites[Adjacency::Count] =
{
    2,
    3,
    0,
    1,
};

internal void
BuildPatternsIndex( const u32 N, State* state, MemoryArena* arena )
{
    u32 patternCount = state->patterns.count;

    for( u32 d = 0; d < ARRAYCOUNT(state->patternsIndex); ++d )
    {
        Array<IndexEntry> entries( arena, patternCount );

        for( u32 p = 0; p < patternCount; ++p )
        {
            u8* pData = state->patterns[p].data;

            Array<bool> matches( arena, patternCount );

            for( u32 q = 0; q < patternCount; ++q )
            {
                u8* qData = state->patterns[q].data;
                matches.Add( Matches( pData, cellDeltas[d], qData, N ) );
            }
            entries.Add( { matches } );
        }
        state->patternsIndex[d] = { entries };
    }
}

inline u32
CountMatches( const IndexEntry& entry )
{
    u32 result = 0;

    for( u32 i = 0; i < entry.matches.count; ++i )
        if( entry.matches[i] )
            result++;

    return result;
}

internal void
Init( State* state, const v2i outputDim, MemoryArena* arena )
{
    u32 waveLength = outputDim.x * outputDim.y;
    u32 patternCount = state->patterns.count;

    state->remaining = waveLength;
    // Increase as needed
    state->propagationStack = Array<BannedTuple>( arena, waveLength * 2 );

    state->wave = Array2<bool>( arena, waveLength, patternCount );
    state->adjacencyCounters = Array2<AdjacencyCounters>( arena, waveLength, patternCount );
    state->compatiblesCount = Array<u32>( arena, waveLength );
    state->weights = Array<r64>( arena, patternCount );
    state->sumFrequencies = Array<r64>( arena, waveLength );
    state->sumWeights = Array<r64>( arena, waveLength );
    state->entropies = Array<r64>( arena, waveLength );
    // FIXME This should actually be (width - N + 1) * (height - N + 1)
    state->observations = Array<u32>( arena, waveLength );

    r64 sumFrequencies = 0;                                                     // sumOfWeights
    r64 sumWeights = 0;                                                         // sumOfWeightLogWeights

    Array<u32> frequencies = state->patternsHash.Values( arena );               // weights
    ASSERT( frequencies.count == patternCount );
    for( u32 p = 0; p < patternCount; ++p )
    {
        state->weights[p] = frequencies[p] * Log( frequencies[p] );
        sumFrequencies += frequencies[p];
        sumWeights += state->weights[p];
    }

    r64 initialEntropy = Log( sumFrequencies ) - sumWeights / sumFrequencies;

    for( u32 i = 0; i < waveLength; ++i )
    {
        state->compatiblesCount[i] = patternCount;
        state->sumFrequencies[i] = sumFrequencies;
        state->sumWeights[i] = sumWeights;
        state->entropies[i] = initialEntropy;
        state->observations[i] = U32MAX;

        for( u32 p = 0; p < patternCount; ++p )
        {
            state->wave.At( i, p ) = true;
            for( u32 d = 0; d < Adjacency::Count; ++d )
                state->adjacencyCounters.At( i, p ).dir[d] = CountMatches( state->patternsIndex[opposites[d]].entries[p] );
        }
    }
}

internal u32
RandomSelect( Array<r32>& distribution )
{
    r32 r = RandomNormalizedR32();

    r32 sum = 0.f;
    for( u32 i = 0; i < distribution.count; ++i )
        sum += distribution[i];

    ASSERT( sum != 0.f );

    for( u32 i = 0; i < distribution.count; ++i )
        distribution[i] /= sum;

    u32 i = 0;
    r32 x = 0.f;

    while( i < distribution.count )
    {
        x += distribution[i];
        if (r <= x)
            break;
        i++;
    }

    return i;
}

internal void
BanPatternAtCell( u32 cellIndex, u32 patternIndex, State* state )
{
    state->wave.At( cellIndex, patternIndex ) = false;

    // Update adjacency counters
    for( u32 d = 0; d < Adjacency::Count; ++d )
        state->adjacencyCounters.At( cellIndex, patternIndex ).dir[d] = 0;

    // Add entry to stack
    state->propagationStack.Push( { cellIndex, patternIndex } );

    // Weird entropy calculation
    u32 i = cellIndex;
    state->entropies[i] += state->sumWeights[i] / state->sumFrequencies[i] - Log( state->sumFrequencies[i] );

    u32* frequency = state->patternsHash.Find( state->patterns[patternIndex] );
    state->compatiblesCount[i]--;
    state->sumFrequencies[i] -= *frequency;
    state->sumWeights[i] -= state->weights[patternIndex];

    state->entropies[i] -= state->sumWeights[i] / state->sumFrequencies[i] - Log( state->sumFrequencies[i] );
}

inline bool
OnBoundary( const v2i& p, const Spec& spec )
{
    const int N = spec.N;
    return !spec.periodic && (p.x < 0 || p.y < 0 || p.x + N > spec.outputDim.x || p.y + N > spec.outputDim.y);
}

inline v2i
PositionFromIndex( u32 index, u32 stride )
{
    return { (i32)(index % stride), (i32)(index / stride) };
}

inline u32
IndexFromPosition( const v2i& p, u32 stride )
{
    return p.y * stride + p.x;
}

internal Result
Observe( const Spec& spec, State* state, MemoryArena* arena )
{
    Result result = InProgress;

    u32 selectedCellIndex = 0;
    r64 minEntropy = R64INF;

    u32 waveLength = spec.outputDim.x * spec.outputDim.y;
    for( u32 i = 0; i < waveLength; ++i )
    {
        if( OnBoundary( PositionFromIndex( i, spec.outputDim.x ), spec ) )
            continue;

        u32 count = state->compatiblesCount[i];
        if( count == 0)
        {
            result = Contradiction;
            break;
        }
        else if( count == 1 )
        {
            u32 p = 0;
            for( ; p < state->patterns.count; ++p )
                if( state->wave.At( i, p ) )
                    break;

            if( state->observations[i] == U32MAX )
            {
                state->observations[i] = p;
                state->remaining--;
            }
            else
                ASSERT( state->observations[i] == p );
        }
        else
        {
            r64 entropy = state->entropies[i];
            if( entropy <= minEntropy )
            {
                r64 noise = 1E-6 * RandomNormalizedR64();
                if( entropy + noise < minEntropy )
                {
                    minEntropy = entropy + noise;
                    selectedCellIndex = i;
                }
            }
        }
    }

    if( result == InProgress )
    {
        if( minEntropy == R64INF )
        {
            // We're done
            result = Done;
        }
        else
        {
            u32 patternCount = state->patterns.count;

            // TODO Store this in the state so we don't have to keep doing it and don't LEAK memory everytime
            Array<u32> frequencies = state->patternsHash.Values( arena );
            Array<r32> distribution = Array<r32>( arena, patternCount );

            for( u32 p = 0; p < patternCount; ++p )
                distribution.Add( state->wave.At( selectedCellIndex, p ) ? frequencies[p] : 0.f );

            u32 selectedPattern = RandomSelect( distribution );

            for( u32 p = 0; p < patternCount; ++p )
            {
                if( state->wave.At( selectedCellIndex, p ) && p != selectedPattern )
                    BanPatternAtCell( selectedCellIndex, p, state );
            }
        }
    }

    return result;
}

internal void
Propagate( const Spec& spec, State* state )
{
    u32 width = spec.outputDim.x;
    u32 height = spec.outputDim.y;
    u32 patternCount = state->patterns.count;

    while( state->propagationStack.count > 0 )
    {
        BannedTuple ban = state->propagationStack.Pop();
        v2i pCell = PositionFromIndex( ban.cellIndex, width );

        for( u32 d = 0; d < Adjacency::Count; ++d )
        {
            v2i pAdjacent = pCell + cellDeltas[d];

            if( OnBoundary( pAdjacent, spec ) )
                continue;

            if( pAdjacent.x < 0 )
                pAdjacent.x += width;
            else if( (u32)pAdjacent.x >= width )
                pAdjacent.x -= width;
            if( pAdjacent.y < 0 )
                pAdjacent.y += height;
            else if( (u32)pAdjacent.y >= height )
                pAdjacent.y -= height;

            u32 adjacentIndex = IndexFromPosition( pAdjacent, width );
            IndexEntry& bannedEntry = state->patternsIndex[d].entries[ban.patternIndex];

            for( u32 p = 0; p < patternCount; ++p )
            {
                if( bannedEntry.matches[p] )
                {
                    AdjacencyCounters& counters = state->adjacencyCounters.At( adjacentIndex, p );
                    counters.dir[d]--;

                    if( counters.dir[d] == 0 )
                        BanPatternAtCell( adjacentIndex, p, state );
                }
            }
        }

        if( spec.displayMode )
            break;
    }
}

internal Texture
CreateOutputTexture( const Spec& spec, State* state, MemoryArena* arena )
{
    if( !state->outputImage )
    {
        state->outputImage = PUSH_ARRAY( arena, spec.outputDim.x * spec.outputDim.y, u32 );
    }

    const u32 N = spec.N;
    const u32 width = spec.outputDim.x;
    const u32 height = spec.outputDim.y;
    u32* out = state->outputImage;

    if( state->currentResult > InProgress )
    {
        for( u32 y = 0; y < height; ++y )
        {
            u32 cellY = (y < height - N + 1) ? 0 : N - 1;
            u32 dy = (y < height - N + 1) ? 0 : y + N - height;

            for( u32 x = 0; x < width; ++x )
            {
                u32 cellX = (x < width - N + 1) ? 0 : N - 1;
                u32 dx = (x < width - N + 1) ? 0 : x + N - width;

                u32 patternIndex = state->observations[cellY * width + cellX];
                u8* patternSamples = state->patterns[patternIndex].data;
                u8 sample = patternSamples[dy * N + dx];

                *out++ = state->palette[sample];
            }
        }
    }
    else if( state->currentResult == InProgress )
    {
        for( u32 y = 0; y < height; ++y )
        {
            for( u32 x = 0; x < width; ++x )
            {
                int contributors = 0, r = 0, g = 0, b = 0;
                u32 color = 0;

                for( u32 dy = 0; dy < N; ++dy )
                {
                    int cellY = (int)(y - dy);
                    if( cellY < 0 )
                        cellY += height;

                    for( u32 dx = 0; dx < N; ++dx )
                    {
                        int cellX = (int)(x - dx);
                        if( cellX < 0 )
                            cellX += width;

                        if( OnBoundary( { cellX, cellY }, spec ) )
                            continue;

                        for( u32 p = 0; p < state->patterns.count; ++p )
                        {
                            if( state->wave.At( cellY * width + cellX, p ) )
                            {
                                u32 cr, cg, cb;
                                u8 sample = state->patterns[p].data[dy * N + dx];
                                UnpackRGBA( state->palette[sample], &cr, &cg, &cb );
                                r += cr;
                                g += cg;
                                b += cb;
                                contributors++;
                            }
                        }
                    }
                }

                if( contributors )
                    color = PackRGBA( r / contributors, g / contributors, b / contributors, 0xFF );

                *out++ = color;
            }
        }
    }

    state->outputTextureHandle = globalPlatform.AllocateTexture( state->outputImage, spec.outputDim.x, spec.outputDim.y, false,
                                                                 state->outputTextureHandle );
    Texture result;
    result.handle = state->outputTextureHandle;
    result.imageBuffer = state->outputImage;
    result.width = spec.outputDim.x;
    result.height = spec.outputDim.y;
    result.channelCount = 4;

    return result;
}

internal void
DrawTest( const Spec& spec, State* state, MemoryArena* arena, MemoryArena* tmpArena, RenderCommands* renderCommands )
{
    const r32 patternScale = 16;

    ImGui::SetNextWindowPos( { 100.f, 100.f }, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( spec.displayDim, ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );
    ImGui::Begin( "window_WFC", NULL,
                  //ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::BeginChild( "child_wfc_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
    // TODO Draw indexed & de-indexed image to help detect errors
    ImGui::Image( spec.source.handle, ImVec2( (r32)spec.source.width * patternScale, (r32)spec.source.height * patternScale ) );

    ImGui::Dummy( ImVec2( 0, 100 ) );
    ImGui::Separator();
    ImGui::Dummy( ImVec2( 0, 20 ) );

    ImGui::BeginGroup();
    if( ImGui::Button( "Output" ) )
        state->currentPage = TestPage::Output;
    if( ImGui::Button( "Patterns" ) )
        state->currentPage = TestPage::Patterns;
    if( ImGui::Button( "Index" ) )
        state->currentPage = TestPage::Index;
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::SameLine();

    u32 patternsCountSqrt = (u32)Round( Sqrt( (r32)state->patterns.count ) );

    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( state->currentPage )
    {
        case TestPage::Output:
        {
			if (state->currentResult > NotStarted)
            {
                r32 outputScale = 4.f;
                ImVec2 imageSize = spec.outputDim * outputScale;

                if( state->remaining != state->lastRemaining )
                {
                    state->outputTexture = CreateOutputTexture( spec, state, arena );
                    state->lastRemaining = state->remaining;
                }

                ImGui::Image( state->outputTexture.handle, imageSize );

                ImGui::SameLine();
                ImGui::Indent();
                ImGui::Text( "Remaining: %d", state->remaining );
            }
        } break;

        case TestPage::Patterns:
        {
            u32 row = 0;
            for( u32 i = 0; i < state->patterns.count; ++i )
            {
                Pattern& p = state->patterns[i];

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

        case TestPage::Index:
        {
            ImGui::BeginChild( "child_index_entries", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
            for( u32 i = 0; i < state->patterns.count; ++i )
            {
                Pattern& p = state->patterns[i];

                if( !p.texture.handle )
                    p.texture = CreatePatternTexture( p, state->palette, arena );

                if( ImGui::ImageButton( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) ) )
                    state->currentIndexEntry = i;
                ImGui::Spacing();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild( "child_index_offsets", ImVec2( 0, 0 ), false, 0 );
            if( state->currentResult > NotStarted )
            {
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

                        u32 row = 0;
                        if( d >= 0 )
                        {
                            Array<bool>& matches = state->patternsIndex[d].entries[state->currentIndexEntry].matches;

                            for( u32 m = 0; m < state->patterns.count; ++m )
                            {
                                if( !matches[m] )
                                    continue;

                                Pattern& p = state->patterns[m];

                                //if( !p.texture.handle )
                                //p.texture = CreatePatternTexture( p, state->palette, arena );

                                ImGui::Image( p.texture.handle, ImVec2( (r32)p.texture.width * patternScale, (r32)p.texture.height * patternScale ) );

                                row++;
                                if( row < patternsCountSqrt )
                                    ImGui::SameLine( 0, 30 );
                                else
                                {
                                    //ImGui::Spacing();
                                    row = 0;
                                }
                            }
                        }

                        if( row == 0 )
                            ImGui::Dummy( ImVec2( 100.f, 100.f ) );

                        ImGui::NextColumn();
                    }
                }
                ImGui::Columns( 1 );
                ImGui::Separator();
            }
            ImGui::EndChild();
        } break;
    }
    ImGui::EndChild();

    ImGui::End();
}


void
DoWFC( const Spec& spec, State* state, MemoryArena* arena, MemoryArena* tmpArena, RenderCommands* renderCommands )
{
    // TODO Investigate how to create temporary arenas that last more than a frame for this kind of thing
    if( state->currentResult == NotStarted )
    {
        PalettizeSource( spec.source, state, arena );
        BuildPatternsFromSource( spec.N, { spec.source.width, spec.source.height }, state, arena );
        BuildPatternsIndex( spec.N, state, arena );

        Init( state, spec.outputDim, arena );

        state->currentResult = InProgress;
    }

    if( state->currentResult == InProgress )
    {
        if( spec.displayMode )
        {
            if( state->propagationStack.count )
                Propagate( spec, state );
            else
                Observe( spec, state, arena );
        }
        else
        {
            while( state->currentResult == InProgress )
            {
                Observe( spec, state, arena );
                Propagate( spec, state );
            }
        }
    }

    DrawTest( spec, state, arena, tmpArena, renderCommands );
}

} // namespace WFC
