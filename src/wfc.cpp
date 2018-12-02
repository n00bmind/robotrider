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
ExtractPattern( u8* input, u32 x, u32 y, const v2i& inputDim, u32 N, u8* output )
{
    u8* dst = output;
    for( u32 j = 0; j < N; ++j )
    {
        u32 iy = (y + j) % inputDim.y;
        for( u32 i = 0; i < N; ++i )
        {
            u32 ix = (x + i) % inputDim.x;
            u8* src = input + iy * inputDim.x + ix;
            *dst++ = *src;
        }
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

    // NOTE We assume 'periodicInput' to be true
    for( u32 j = 0; j < (u32)inputDim.y; ++j )
    {
        for( u32 i = 0; i < (u32)inputDim.x; ++i )
        {
            ExtractPattern( state->input, i, j, inputDim, N, patternData );
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
    result.handle = globalPlatform.AllocateOrUpdateTexture( (u8*)imageBuffer, pattern.stride, pattern.stride, false, nullptr );
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
        Array<IndexEntry> entries( arena, 0, patternCount );

        for( u32 p = 0; p < patternCount; ++p )
        {
            u8* pData = state->patterns[p].data;

            Array<bool> matches( arena, 0, patternCount );

            for( u32 q = 0; q < patternCount; ++q )
            {
                u8* qData = state->patterns[q].data;
                matches.Push( Matches( pData, cellDeltas[d], qData, N ) );
            }
            entries.Push( { matches } );
        }
        state->patternsIndex[d] = { entries };
    }
}

internal void
AllocSnapshot( Snapshot* result, u32 waveLength, u32 patternCount, MemoryArena* arena )
{
    result->wave = Array2<bool>( arena, waveLength, patternCount );
    result->adjacencyCounters = Array2<AdjacencyCounters>( arena, waveLength, patternCount );
    result->compatiblesCount = Array<u32>( arena, waveLength, waveLength );
    result->sumFrequencies = Array<r64>( arena, waveLength, waveLength );
    result->sumWeights = Array<r64>( arena, waveLength, waveLength );
    result->entropies = Array<r64>( arena, waveLength, waveLength );
    result->distribution = Array<r32>( arena, patternCount, patternCount );
}

internal void
CopySnapshot( Snapshot* source, Snapshot* target )
{
    source->wave.CopyTo( &target->wave );
    source->adjacencyCounters.CopyTo( &target->adjacencyCounters );
    source->compatiblesCount.CopyTo( &target->compatiblesCount );
    source->sumFrequencies.CopyTo( &target->sumFrequencies );
    source->sumWeights.CopyTo( &target->sumWeights );
    source->entropies.CopyTo( &target->entropies );
    source->distribution.CopyTo( &target->distribution );
}

internal void
Init( const Spec& spec, State* state, MemoryArena* arena )
{
    TIMED_BLOCK

    state->arena = arena;

    // Process input data
    PalettizeSource( spec.source, state, arena );
    BuildPatternsFromSource( spec.N, { spec.source.width, spec.source.height }, state, arena );
    BuildPatternsIndex( spec.N, state, arena );

    // Init global state
    // FIXME This should actually be (width - N + 1) * (height - N + 1) ?
    u32 waveLength = spec.outputDim.x * spec.outputDim.y;
    u32 patternCount = state->patterns.count;
    state->frequencies = state->patternsHash.Values( arena );
    ASSERT( state->frequencies.count == patternCount );
    state->weights = Array<r64>( arena, patternCount, patternCount );
    const u32 propagationStackSize = waveLength * 2;                            // Increase as needed
    state->propagationStack = Array<BannedTuple>( arena, 0, propagationStackSize );
    state->distributionTemp = Array<r32>( arena, patternCount, patternCount );

    // Create initial snapshot
    Snapshot snapshot0;
    sz lowWaterMark = arena->used;

    AllocSnapshot( &snapshot0, waveLength, patternCount, arena );

    // So we want to calculate the max number of snapshots that will fit in our arena,
    // but we still need to allocate the snapshot stack, whose size depends on the number of snapshots
    // Now, given:
    sz S = arena->used - lowWaterMark;                                          // Size in memory for all the snapshot's data
    sz A = arena->size - lowWaterMark;                                          // Total available for all snapshots + stack
    sz s = sizeof(Snapshot);                                                    // Size of the snapshot struct itself
    // We find that:
    // The size of the snapshot stack content (barring the struct itself) would be x = n * s, where n is the number of snapshots
    // Conversely, n = (A - x) / S
    // So if we substitute x and solve for n we have:
    u32 maxSnapshotCount = SafeR64ToU32( (r32)A / (S + s) );
    ASSERT( maxSnapshotCount * s + maxSnapshotCount * S < A );

    // We need to allocate the stack before any snapshot, so that rewinding snapshots will work properly
    RewindArena( arena, lowWaterMark );
    state->snapshotStack = RingStack<Snapshot>( arena, maxSnapshotCount );
    AllocSnapshot( &snapshot0, waveLength, patternCount, arena );
    state->currentSnapshot = state->snapshotStack.Push( snapshot0 );

    // Calc initial entropy, counters, etc.
    r64 sumFrequencies = 0;                                                     // sumOfWeights
    r64 sumWeights = 0;                                                         // sumOfWeightLogWeights
    for( u32 p = 0; p < patternCount; ++p )
    {
        state->weights[p] = state->frequencies[p] * Log( state->frequencies[p] );
        sumFrequencies += state->frequencies[p];
        sumWeights += state->weights[p];
    }
    r64 initialEntropy = Log( sumFrequencies ) - sumWeights / sumFrequencies;

    for( u32 i = 0; i < waveLength; ++i )
    {
        snapshot0.compatiblesCount[i] = patternCount;
        snapshot0.sumFrequencies[i] = sumFrequencies;
        snapshot0.sumWeights[i] = sumWeights;
        snapshot0.entropies[i] = initialEntropy;

        for( u32 p = 0; p < patternCount; ++p )
        {
            snapshot0.wave.At( i, p ) = true;
            for( u32 d = 0; d < Adjacency::Count; ++d )
            {
                IndexEntry& entry = state->patternsIndex[opposites[d]].entries[p];

                u32 matches = 0;
                for( u32 m = 0; m < entry.matches.count; ++m )
                    if( entry.matches[m] )
                        matches++;

                snapshot0.adjacencyCounters.At( i, p ).dir[d] = matches;
            }
        }
    }

    state->remainingObservations = waveLength;
    state->currentResult = InProgress;
}

internal bool
RandomSelect( const Array<r32>& distribution, Array<r32>* temp, u32* selection )
{
    bool result = false;

    r32 sum = 0.f;
    for( u32 i = 0; i < distribution.count; ++i )
        sum += distribution[i];

    r32* d = temp->data;
    r32 r = RandomNormalizedR32();
    if( sum != 0.f )
    {
        for( u32 i = 0; i < distribution.count; ++i )
            d[i] = distribution[i] / sum;

        u32 i = 0;
        r32 x = 0.f;

        while( i < distribution.count )
        {
            x += d[i];
            if (r <= x)
                break;
            i++;
        }
        *selection = i;
        result = true;
    }

    return result;
}

internal bool
BanPatternAtCell( u32 cellIndex, u32 patternIndex, State* state )
{
    bool result = true;
    Snapshot* snapshot = state->currentSnapshot;

    snapshot->wave.At( cellIndex, patternIndex ) = false;

    for( u32 d = 0; d < Adjacency::Count; ++d )
        snapshot->adjacencyCounters.At( cellIndex, patternIndex ).dir[d] = 0;

    snapshot->compatiblesCount[cellIndex]--;
    if( snapshot->compatiblesCount[cellIndex] == 0 )
        result = false;

    // Weird entropy calculation
    u32 i = cellIndex;
    snapshot->entropies[i] += snapshot->sumWeights[i] / snapshot->sumFrequencies[i] - Log( snapshot->sumFrequencies[i] );

    // TODO This is now stored in 'frequencies'
    u32* frequency = state->patternsHash.Find( state->patterns[patternIndex] );
    snapshot->sumFrequencies[i] -= *frequency;
    snapshot->sumWeights[i] -= state->weights[patternIndex];

    snapshot->entropies[i] -= snapshot->sumWeights[i] / snapshot->sumFrequencies[i] - Log( snapshot->sumFrequencies[i] );

    // Add entry to stack
    if( result )
        state->propagationStack.Push( { cellIndex, patternIndex } );

    return result;
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
    Snapshot* snapshot = state->currentSnapshot;

    u32 observedCellIndex = 0;
    r64 minEntropy = R64INF;

    u32 observations = 0;
    u32 waveLength = spec.outputDim.x * spec.outputDim.y;
    for( u32 i = 0; i < waveLength; ++i )
    {
        if( OnBoundary( PositionFromIndex( i, spec.outputDim.x ), spec ) )
            continue;

        u32 count = snapshot->compatiblesCount[i];
        if( count == 0)
        {
            result = Contradiction;
            break;
        }
        else if( count == 1 )
        {
            observations++;
        }
        else
        {
            r64 entropy = snapshot->entropies[i];
            if( entropy <= minEntropy )
            {
                r64 noise = 1E-6 * RandomNormalizedR64();
                if( entropy + noise < minEntropy )
                {
                    minEntropy = entropy + noise;
                    observedCellIndex = i;
                }
            }
        }
    }
    state->remainingObservations = snapshot->compatiblesCount.maxCount - observations;

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

            for( u32 p = 0; p < patternCount; ++p )
                snapshot->distribution[p] = (snapshot->wave.At( observedCellIndex, p ) ? state->frequencies[p] : 0.f);

            u32 observedDistributionIndex;
            bool haveSelection = RandomSelect( snapshot->distribution, &state->distributionTemp, &observedDistributionIndex );
            ASSERT( haveSelection );

            snapshot->lastObservedCellIndex = observedCellIndex;
            snapshot->lastObservedDistributionIndex = observedDistributionIndex;

            // Get a new snapshot from the stack
            // NOTE If the snapshot space was previously allocated, use that same space, otherwise allocate
            snapshot = state->snapshotStack.Push( false );
            if( !snapshot->wave )
                AllocSnapshot( snapshot, waveLength, patternCount, arena );
            CopySnapshot( state->currentSnapshot, snapshot );

            state->currentSnapshot = snapshot;

            for( u32 p = 0; p < patternCount; ++p )
            {
                if( snapshot->wave.At( observedCellIndex, p ) && p != observedDistributionIndex )
                {
                    bool compatiblesRemaining = BanPatternAtCell( observedCellIndex, p, state );
                    if( !compatiblesRemaining )
                    {
                        result = Contradiction;
                        break;
                    }
                }
            }
        }
    }

    return result;
}

internal Result
Propagate( const Spec& spec, State* state )
{
    Result result = InProgress;

    u32 width = spec.outputDim.x;
    u32 height = spec.outputDim.y;
    u32 patternCount = state->patterns.count;

    while( result == InProgress && state->propagationStack.count > 0 )
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
                    AdjacencyCounters& counters = state->currentSnapshot->adjacencyCounters.At( adjacentIndex, p );
                    counters.dir[d]--;

                    if( counters.dir[d] == 0 )
                    {
                        bool compatiblesRemaining = BanPatternAtCell( adjacentIndex, p, state );
                        if( !compatiblesRemaining )
                        {
                            result = Contradiction;
                            break;
                        }
                    }
                }
            }
        }
    }

    return result;
}

internal Result
RewindSnapshot( State* state )
{
    Result result = Contradiction;
    Snapshot* snapshot = state->currentSnapshot;

    // TODO Better compress this snapshot handling business, to make all this less confusing
#if 0
    Sleep( 500 );
#endif
    bool haveNewSelection = false;
    while( snapshot && !haveNewSelection )
    {
        // Get previous snapshot from the stack, if available
        state->snapshotStack.Pop();
        snapshot = state->snapshotStack.Top();

        if( snapshot )
        {
            // Discard the random selection we chose last time, and try a different one
            snapshot->distribution[snapshot->lastObservedDistributionIndex] = 0.f;
            haveNewSelection = RandomSelect( snapshot->distribution, &state->distributionTemp, &snapshot->lastObservedDistributionIndex );
        }
    }

    if( haveNewSelection )
    {
        // We're back on track
        result = InProgress;
        state->currentSnapshot = snapshot;

        u32 observedCellIndex = snapshot->lastObservedCellIndex;
        u32 observedDistributionIndex = snapshot->lastObservedDistributionIndex;

        // Create a brand new snapshot that stems from the new selection
        // (since we've at least backtracked one, this one _has_ to be allocated already)
        snapshot = state->snapshotStack.Push( false );
        CopySnapshot( state->currentSnapshot, snapshot );
        state->currentSnapshot = snapshot;

        for( u32 p = 0; p < state->patterns.count; ++p )
        {
            if( snapshot->wave.At( observedCellIndex, p ) && p != observedDistributionIndex )
            {
                bool compatiblesRemaining = BanPatternAtCell( observedCellIndex, p, state );
                if( !compatiblesRemaining )
                {
                    result = Contradiction;
                    break;
                }
            }
        }
    }
    else
        result = Failed;

    return result;
}

internal bool
IsFinished( const State& state )
{
    return state.currentResult >= Cancelled;
}

void
StartWFCSync( const Spec& spec, State* state, MemoryArena* wfcArena )
{
    Init( spec, state, wfcArena ); 

    while( !IsFinished( *state ) )
    {
        if( state->cancellationRequested )
            state->currentResult = Cancelled;

        if( state->currentResult == InProgress )
            state->currentResult = Observe( spec, state, wfcArena );

        if( state->currentResult == Contradiction )
            state->currentResult = RewindSnapshot( state );

        if( state->currentResult == InProgress )
            state->currentResult = Propagate( spec, state );
    }
}

internal
PLATFORM_JOBQUEUE_CALLBACK(DoWFC)
{
    WFCJob* job = (WFCJob*)userData;
    StartWFCSync( job->spec, job->state, job->arena );
}

const State*
StartWFCAsync( const Spec& spec, MemoryArena* wfcArena )
{
    const State* result = PUSH_STRUCT( wfcArena, State );

    // We may want to have this passed independently?
    WFCJob* job = PUSH_STRUCT( wfcArena, WFCJob );
    *job =
    {
        spec,
        (State*)result,     // const for the starting thread only
        wfcArena,
    };

    globalPlatform.AddNewJob( globalPlatform.hiPriorityQueue,
                              DoWFC,
                              job );

    return result;
}


internal void
CreateOutputTexture( const Spec& spec, const State* state, u32* imageBuffer, Texture* result )
{
    const u32 N = spec.N;
    const u32 width = spec.outputDim.x;
    const u32 height = spec.outputDim.y;
    u32* out = imageBuffer;

    Snapshot* snapshot = state->currentSnapshot;
    if( state->currentResult == InProgress )
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
                            if( snapshot->wave.At( cellY * width + cellX, p ) )
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
    else if( state->currentResult == Done )
    {
        for( u32 y = 0; y < height; ++y )
        {
            u32 cellY = (y < height - N + 1) ? y : y - N + 1;
            u32 dy = (y < height - N + 1) ? 0 : y + N - height;

            for( u32 x = 0; x < width; ++x )
            {
                u32 cellX = (x < width - N + 1) ? x : x - N + 1;
                u32 dx = (x < width - N + 1) ? 0 : x + N - width;

                u32 color = 0;
                u32 patternIndex = U32MAX;
                u32 cellIndex = cellY * width + cellX;
                for( u32 p = 0; p < state->patterns.count; ++p )
                {
                    if( snapshot->wave.At( cellIndex, p ) )
                    {
                        patternIndex = p;
                        break;
                    }
                }
                ASSERT( patternIndex != U32MAX );

                u8* patternSamples = state->patterns[patternIndex].data;
                u8 sample = patternSamples[dy * N + dx];
                color = state->palette[sample];

                ASSERT( color != 0 );
                *out++ = color;
            }
        }
    }
    else if( state->currentResult == Contradiction || state->currentResult == Failed )
    {
        for( u32 y = 0; y < height; ++y )
        {
            for( u32 x = 0; x < width; ++x )
            {
                u32 color = *out;
                v4 unpacked = UnpackRGBAToV401( color );
                unpacked = Hadamard( unpacked, { .5f, .5f, .5f, 1.f } );
                color = Pack01ToRGBA( unpacked );

                u32 count = snapshot->compatiblesCount[y * width + x];
                if( count == 0 )
                    color = Pack01ToRGBA( 1, 0, 1, 1 );

                *out++ = color;
            }
        }
    }

    // Use existing handle if we have one
    result->handle = globalPlatform.AllocateOrUpdateTexture( imageBuffer, spec.outputDim.x, spec.outputDim.y, false,
                                                             result->handle );
    result->imageBuffer = imageBuffer;
    result->width = spec.outputDim.x;
    result->height = spec.outputDim.y;
    result->channelCount = 4;
}

internal u32
DrawFileSelectorPopup( const Array<Spec>& specs, u32 currentSpecIndex )
{
    u32 result = U32MAX;

    ImGui::Text( "Specs in wfc.vars" );
    ImGui::Separator();

    ImGui::BeginChild( "child_file_list", ImVec2( 250, 400 ) );

    for( u32 i = 0; i < specs.count; ++i )
    {
        bool selected = i == currentSpecIndex;
        if( ImGui::MenuItem( specs[i].name, nullptr, selected ) )
        {
            result = i;
        }
    }
    ImGui::EndChild();

    return result;
}


u32
DrawTest( const Array<Spec>& specs, const State* state, DisplayState* displayState, const v2& displayDim,
          const DebugState* debugState, MemoryArena* wfcDisplayArena, MemoryArena* tmpArena )
{
    u32 selectedIndex = U32MAX;
    const Spec& spec = specs[displayState->currentSpecIndex];

    const r32 outputScale = 4.f;
    const r32 patternScale = outputScale * 2;

    ImGui::SetNextWindowPos( { 100.f, 100.f }, ImGuiCond_Always );
    ImGui::SetNextWindowSize( displayDim, ImGuiCond_Always );
    ImGui::SetNextWindowSizeConstraints( ImVec2( -1, -1 ), ImVec2( -1, -1 ) );
    ImGui::Begin( "window_WFC", NULL,
                  //ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoMove );

    ImGui::BeginChild( "child_wfc_left", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
    // TODO Draw indexed & de-indexed image to help detect errors
    ImVec2 buttonDim = ImVec2( (r32)spec.source.width * outputScale, (r32)spec.source.height * outputScale );
    if( ImGui::ImageButton( spec.source.handle, buttonDim ) )
        ImGui::OpenPopup( "popup_file_selector" );

    if( ImGui::BeginPopup( "popup_file_selector" ) )
    {
        selectedIndex = DrawFileSelectorPopup( specs, displayState->currentSpecIndex );
        if( selectedIndex != U32MAX )
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    

    ImGui::Dummy( ImVec2( 0, 100 ) );
    ImGui::Separator();
    ImGui::Dummy( ImVec2( 0, 20 ) );

    ImGui::BeginGroup();
    if( ImGui::Button( "Output" ) )
        displayState->currentPage = TestPage::Output;
    if( ImGui::Button( "Patterns" ) )
        displayState->currentPage = TestPage::Patterns;
    if( ImGui::Button( "Index" ) )
        displayState->currentPage = TestPage::Index;
    ImGui::EndGroup();

    ImGui::EndChild();
    ImGui::SameLine();

    const u32 patternsCount = state->patterns.count;
    const u32 patternsCountSqrt = (u32)Round( Sqrt( (r32)patternsCount ) );

    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( displayState->currentPage )
    {
        case TestPage::Output:
        {
			//if (state->currentResult > NotStarted)
            {
                ImGui::BeginChild( "child_output_meta", ImVec2( 300, 0 ) );

                if( state->currentResult == Contradiction )
                    ImGui::Text( "CONTRADICTION!" );
                else if( state->currentResult == Failed )
                    ImGui::Text( "FAILED!" );
                else
                    ImGui::Text( "Remaining: %d", state->remainingObservations );

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text( "Name: %s", spec.name );
                ImGui::Text( "Num. patterns: %d", patternsCount );

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text( "Snapshot stack: %d / %d", state->snapshotStack.buffer.count, state->snapshotStack.buffer.maxCount );

                ImGui::Spacing();
                ImGui::Spacing();
                ImGui::Text( "Total memory: %d / %d", state->arena->used / MEGABYTES(1), state->arena->size / MEGABYTES(1) );

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild( "child_output_image", ImVec2( 1100, 0 ) );
                ImVec2 imageSize = spec.outputDim * outputScale;

                if( state->currentResult != displayState->lastDisplayedResult ||
                    state->remainingObservations != displayState->lastRemainingObservations )
                {
                    // NOTE Very Broken Synchronization, but I don't really want to synchronize..
                    displayState->lastDisplayedResult = state->currentResult;
                    displayState->lastRemainingObservations = state->remainingObservations;

                    if( !displayState->outputImageBuffer )
                        displayState->outputImageBuffer = PUSH_ARRAY( wfcDisplayArena, spec.outputDim.x * spec.outputDim.y, u32 );

                    CreateOutputTexture( spec, state, displayState->outputImageBuffer, &displayState->outputTexture );
                }

                ImGui::Image( displayState->outputTexture.handle, imageSize );

                if( ImGui::IsItemClicked() )
                    selectedIndex = displayState->currentSpecIndex;

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild( "child_debug", ImVec2( 0, 0 ) );
                if( debugState )
                {
                    DrawPerformanceCounters( debugState, tmpArena );
                }
                ImGui::EndChild();
            }
        } break;

        case TestPage::Patterns:
        {
            if( !displayState->patternTextures )
                displayState->patternTextures = Array<Texture>( wfcDisplayArena, patternsCount, patternsCount );

            bool firstRow = true;
            ImGui::Columns( patternsCountSqrt, nullptr, false );
            for( u32 i = 0; i < patternsCount; ++i )
            {
                const Pattern& p = state->patterns[i];
                Texture& t = displayState->patternTextures[i];

                if( !t.handle )
                    t = CreatePatternTexture( p, state->palette, wfcDisplayArena );

                ImGui::Image( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) );
                ImGui::SameLine();
                ImGui::Text( "%u", state->frequencies[i] );

                ImGui::NextColumn();

                if( ImGui::GetColumnIndex() == 0 )
                    firstRow = false;

                if( !firstRow )
                    ImGui::Dummy( V2( 0, 10 ) );
            }
        } break;

        case TestPage::Index:
        {
            if( !displayState->patternTextures )
                displayState->patternTextures = Array<Texture>( wfcDisplayArena, patternsCount, patternsCount );

            ImGui::BeginChild( "child_index_entries", ImVec2( ImGui::GetWindowContentRegionWidth() * 0.1f, 0 ), false, 0 );
            for( u32 i = 0; i < patternsCount; ++i )
            {
                const Pattern& p = state->patterns[i];
                Texture& t = displayState->patternTextures[i];

                if( !t.handle )
                    t = CreatePatternTexture( p, state->palette, wfcDisplayArena );

                if( ImGui::ImageButton( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) ) )
                    displayState->currentIndexEntry = i;
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
                        ImGui::Text("{ %d, %d }", i - 1, j - 1 );

                        if( i == 1 && j == 1 )
                        {
                            Texture& t = displayState->patternTextures[displayState->currentIndexEntry];
                            ImGui::Image( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) );
                        }
                        else if( i == 0 && j == 1 )
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
                            const Array<bool>& matches = state->patternsIndex[d].entries[displayState->currentIndexEntry].matches;

                            for( u32 m = 0; m < patternsCount; ++m )
                            {
                                if( !matches[m] )
                                    continue;

                                const Pattern& p = state->patterns[m];
                                Texture& t = displayState->patternTextures[m];

                                ImGui::Image( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) );

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


    ImGui::ShowDemoWindow();

    return selectedIndex;
}


} // namespace WFC
