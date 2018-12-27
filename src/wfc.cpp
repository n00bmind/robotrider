namespace WFC
{


internal void
PalettizeSource( const Texture& source, State* state, MemoryArena* arena )
{
    state->input = PUSH_ARRAY( arena, source.width * source.height, u8 );

    for( u32 i = 0; i < source.width * source.height; ++i )
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
ExtractPattern( u8* input, u32 x, u32 y, const v2u& inputDim, u32 N, u8* output )
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
        if (state->patternsHash.count <= MaxAdjacencyCount + 1)
        {
            u8* patternData = PUSH_ARRAY(arena, N * N, u8);
            COPY(data, patternData, N * N);
            state->patternsHash.Add({ patternData, N }, 1 );
        }
        else
            LOG("WARN :: Truncating input patterns at %d!", MaxAdjacencyCount);
    }
}

internal void
BuildPatternsFromSource( const u32 N, const v2u& inputDim, State* state, MemoryArena* arena )
{
    u8* patternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* rotatedPatternData = PUSH_ARRAY( arena, N * N, u8 );
    u8* reflectedPatternData = PUSH_ARRAY( arena, N * N, u8 );

    // NOTE Totally arbitrary number for the entry count
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
                matches.Push( Matches( pData, adjacencyMeta[d].cellDelta.xy, qData, N ) );
            }
            entries.Push( { matches } );
        }
        state->patternsIndex[d] = { entries };
    }
}

internal void
AllocSnapshot( Snapshot* result, u32 waveLength, u32 patternCount, MemoryArena* arena )
{
    u32 patternGroupCount = Ceil( (r32)patternCount / 64.f );
    result->wave = Array2<u64>( arena, waveLength, patternGroupCount );
    result->adjacencyCounters = Array2<u64>( arena, waveLength, patternCount );
    result->compatiblesCount = Array<u32>( arena, waveLength, waveLength );
    result->sumFrequencies = Array<r64>( arena, waveLength, waveLength );
    result->sumWeights = Array<r64>( arena, waveLength, waveLength );
    result->entropies = Array<r64>( arena, waveLength, waveLength );
    result->distribution = Array<r32>( arena, patternCount, patternCount );
}

internal void
CopySnapshot( const Snapshot* source, Snapshot* target )
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
SetAdjacencyAt( Snapshot* snapshot, u32 cellIndex, u32 patternIndex, u32 dirIndex, u32 adjacencyCount )
{
    u64 exp = dirIndex * BitsPerAxis;
    u64 value = (u64)adjacencyCount << exp;
    u64 mask = adjacencyMeta[dirIndex].counterMask;

    u64& counter = snapshot->adjacencyCounters.AtRowCol( cellIndex, patternIndex );

    counter = (counter & ~mask) | (value & mask);
}

inline bool
DecreaseAdjacencyAndTestZAt( Snapshot* snapshot, u32 cellIndex, u32 patternIndex, u32 dirIndex )
{
    TIMED_BLOCK;

    u64 exp = dirIndex * BitsPerAxis;
    u64 one = 1ULL << exp;
    u64 mask = adjacencyMeta[dirIndex].counterMask;

    u64& counter = snapshot->adjacencyCounters.AtRowCol( cellIndex, patternIndex );

    // NOTE This has been seen to underflow (trying to prevent that actually seems to break things!)
    u64 value = (counter & mask) - one;
    counter = (counter & ~mask) | (value & mask);

    return value == 0;
}

inline bool
GetWaveAt( const Snapshot* snapshot, u32 cellIndex, u32 patternIndex, const State* state )
{
    ASSERT( patternIndex < state->patterns.count );
    
    const u64* row = snapshot->wave.AtRow( cellIndex );
    u32 patternSubIndex = patternIndex / 64;
    u64 bitset = row[patternSubIndex];
    u32 bitIndex = patternIndex % 64;
    u64 mask = 1ULL << bitIndex;

    return (bitset & mask) != 0;
}

inline void
DisableWaveAt( Snapshot* snapshot, u32 cellIndex, u32 patternIndex, const State* state )
{
    ASSERT( patternIndex < state->patterns.count );

    u64* row = snapshot->wave.AtRow( cellIndex );
    u32 patternSubIndex = patternIndex / 64;
    u64& bitset = row[patternSubIndex];

    u32 bitIndex = patternIndex % 64;
    u64 mask = ~(1ULL << bitIndex);

    bitset = bitset & mask;
}

inline void
ResetWaveAt( Snapshot* snapshot, u32 cellIndex, State* state )
{
    u64* row = snapshot->wave.AtRow( cellIndex );

    u32 groupCount = state->patterns.count / 64;
    for( u32 p = 0; p < groupCount; ++p )
        row[p] = ~0ULL;

    u32 spareCount = state->patterns.count % 64;
    if( spareCount )
        row[groupCount] = (~0ULL); // >> (64 - spareCount);
}

internal void
Init( const Spec& spec, const v2u& pOutputChunk, State* state, MemoryArena* arena )
{
    TIMED_BLOCK;

    state->arena = arena;

    // Process input data
    PalettizeSource( spec.source, state, arena );
    BuildPatternsFromSource( spec.N, { spec.source.width, spec.source.height }, state, arena );
    BuildPatternsIndex( spec.N, state, arena );

    // Init global state
    // FIXME This should actually be (width - N + 1) * (height - N + 1) ?
    u32 waveLength = spec.outputChunkDim.x * spec.outputChunkDim.y;
    u32 patternCount = state->patterns.count;
    state->frequencies = state->patternsHash.Values( arena );
    ASSERT( state->frequencies.count == patternCount );
    state->weights = Array<r64>( arena, patternCount, patternCount );
    const u32 propagationStackSize = waveLength * 2;                            // Increase as needed
    state->propagationStack = Array<BannedTuple>( arena, 0, propagationStackSize );
    state->distributionTemp = Array<r32>( arena, patternCount, patternCount );
    state->backtrackedCellIndices = RingBuffer<u32>( arena, BacktrackedCellsCacheCount );

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

    // Truncate at 64, since having more does actually seems less effective
    maxSnapshotCount = Min( maxSnapshotCount, MaxBacktrackingDepth );

    // We need to allocate the stack before any snapshot, so that rewinding snapshots will work properly
    // TODO Do this using TemporaryMemory and remove RewindArena
    RewindArena( arena, lowWaterMark );
    state->snapshotStack = Array<Snapshot>( arena, 0, maxSnapshotCount );
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
        ResetWaveAt( &snapshot0, i, state );
        snapshot0.compatiblesCount[i] = patternCount;
        snapshot0.sumFrequencies[i] = sumFrequencies;
        snapshot0.sumWeights[i] = sumWeights;
        snapshot0.entropies[i] = initialEntropy;

        for( u32 p = 0; p < patternCount; ++p )
        {
            for( u32 d = 0; d < Adjacency::Count; ++d )
            {
                u32 oppositeIndex = adjacencyMeta[d].oppositeIndex;
                IndexEntry& entry = state->patternsIndex[oppositeIndex].entries[p];

                u32 matches = 0;
                for( u32 m = 0; m < entry.matches.count; ++m )
                    if( entry.matches[m] )
                        matches++;

                SetAdjacencyAt( &snapshot0, i, p, d, matches );
            }
        }
    }

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
    TIMED_BLOCK;

    bool result = true;
    Snapshot* snapshot = state->currentSnapshot;

    DisableWaveAt( snapshot, cellIndex, patternIndex, state );

    for( u32 d = 0; d < Adjacency::Count; ++d )
        SetAdjacencyAt( snapshot, cellIndex, patternIndex, d, 0 );

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
    return !spec.periodic
        && (p.x < 0 || p.y < 0 || p.x + N > (i32)spec.outputChunkDim.x || p.y + N > (i32)spec.outputChunkDim.y);
}

inline v2u
PositionFromIndex( u32 index, u32 stride )
{
    return { (index % stride), (index / stride) };
}

inline u32
IndexFromPosition( const v2u& p, u32 stride )
{
    return p.y * stride + p.x;
}

internal bool
BuildDistributionAndSelectAt( u32 cellIndex, Snapshot* snapshot, State* state, u32* selection )
{
    for( u32 p = 0; p < state->patterns.count; ++p )
        snapshot->distribution[p] = (GetWaveAt( snapshot, cellIndex, p, state ) ? state->frequencies[p] : 0.f);

    bool result = RandomSelect( snapshot->distribution, &state->distributionTemp, selection );
    return result;
}

// NOTE If the snapshot space was previously allocated, uses that same space, otherwise allocates
internal Snapshot*
PushNewSnapshot( Snapshot* source, State* state, MemoryArena* arena )
{
    Snapshot* result = state->snapshotStack.PushEmpty( false );

    if( !result->wave )
        AllocSnapshot( result, source->wave.rows, state->patterns.count, arena );
    CopySnapshot( source, result );

    result->lastObservedDistributionIndex = 0;
    result->lastObservedCellIndex = 0;
    result->lastObservationCount = 0;

    return result;
}

internal bool
ApplyObservedPatternAt( u32 observedCellIndex, u32 observedDistributionIndex, Snapshot* snapshot, State* state )
{
    TIMED_BLOCK;

    bool compatiblesRemaining = true;

    for( u32 p = 0; p < state->patterns.count; ++p )
    {
        if( GetWaveAt( snapshot, observedCellIndex, p, state ) && p != observedDistributionIndex )
        {
            compatiblesRemaining = BanPatternAtCell( observedCellIndex, p, state );
            if( !compatiblesRemaining )
                break;
        }
    }

    return compatiblesRemaining;
}

internal bool
NeedSnapshot( State* state, u32 totalObservationCount )
{
    u32 value = state->observationCount;
    
    r32 x = (r32)state->snapshotStack.count;
    r32 SC = (r32)state->snapshotStack.maxCount;

    // Using a n-th degree parabola, calc the observations count corresponding to the next snapshot
    r32 n = ObservationCountParabolaDegree;
    r32 targetValue = ((r32)totalObservationCount / Pow( SC, n )) * Pow( x, n );

    return value >= targetValue;
}

internal Result
Observe( const Spec& spec, State* state, MemoryArena* arena )
{
    TIMED_BLOCK;

    Result result = InProgress;
    Snapshot* snapshot = state->currentSnapshot;

    u32 observedCellIndex = 0;
    r64 minEntropy = R64INF;

    u32 observations = 0;
    for( u32 i = 0; i < snapshot->wave.rows; ++i )
    {
        if( OnBoundary( V2i( PositionFromIndex( i, spec.outputChunkDim.x ) ), spec ) )
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
            if( entropy <= minEntropy && !state->backtrackedCellIndices.Contains( i ) )
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
    state->observationCount = observations;

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

            u32 observedDistributionIndex;
            bool haveSelection = BuildDistributionAndSelectAt( observedCellIndex, snapshot, state,
                                                               &observedDistributionIndex );
            ASSERT( haveSelection );

            if( NeedSnapshot( state, snapshot->wave.rows ) )
            {
                snapshot->lastObservedCellIndex = observedCellIndex;
                snapshot->lastObservedDistributionIndex = observedDistributionIndex;
                snapshot->lastObservationCount = state->observationCount;

                snapshot = PushNewSnapshot( state->currentSnapshot, state, arena );
                state->currentSnapshot = snapshot;
            }

            bool compatiblesRemaining = ApplyObservedPatternAt( observedCellIndex, observedDistributionIndex, snapshot, state );
            if( !compatiblesRemaining )
                result = Contradiction;
        }
    }

    return result;
}

internal Result
Propagate( const Spec& spec, State* state )
{
    Result result = InProgress;

    u32 width = spec.outputChunkDim.x;
    u32 height = spec.outputChunkDim.y;
    u32 patternCount = state->patterns.count;

    while( result == InProgress && state->propagationStack.count > 0 )
    {
        TIMED_BLOCK;

        BannedTuple ban = state->propagationStack.Pop();
        v2i pCell = V2i( PositionFromIndex( ban.cellIndex, width ) );

        for( u32 d = 0; d < Adjacency::Count; ++d )
        {
            v2i pAdjacent = pCell + adjacencyMeta[d].cellDelta.xy;

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

            u32 adjacentIndex = IndexFromPosition( V2u( pAdjacent ), width );
            IndexEntry& bannedEntry = state->patternsIndex[d].entries[ban.patternIndex];

            for( u32 p = 0; p < patternCount; ++p )
            {
                if( bannedEntry.matches[p] )
                {
                    bool isZero = DecreaseAdjacencyAndTestZAt( state->currentSnapshot, adjacentIndex, p, d );
                    if( isZero )
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
    TIMED_BLOCK;

    Result result = Contradiction;
    Snapshot* snapshot = state->currentSnapshot;
    u32 patternCount = state->patterns.count;

    state->propagationStack.Clear();

    bool haveNewSelection = false;
    while( snapshot && !haveNewSelection )
    {
        // Get previous snapshot from the stack, if available
        state->snapshotStack.Pop();
        snapshot = &state->snapshotStack.Last();
        if( snapshot )
        {
            // Discard the random selection we chose last time, and try a different one
            snapshot->distribution[snapshot->lastObservedDistributionIndex] = 0.f;
            haveNewSelection = RandomSelect( snapshot->distribution, &state->distributionTemp, &snapshot->lastObservedDistributionIndex );

            if( !haveNewSelection )
            {
                if( BacktrackedCellsCacheCount )
                {
                    // Discard this cell index entirely and try a different one
                    // TODO Do we somehow want to retry distribution options once we discard new cells?
                    state->backtrackedCellIndices.Push( snapshot->lastObservedCellIndex );
                }
#if 0
                Sleep( 250 );
#endif
            }
        }
    }

    if( haveNewSelection )
    {
        state->currentSnapshot = snapshot;

        u32 observedCellIndex = snapshot->lastObservedCellIndex;
        u32 observedDistributionIndex = snapshot->lastObservedDistributionIndex;

        // NOTE We know we have backtracked at least once, so the storage for the snapshot is guaranteed to be allocated
        snapshot = PushNewSnapshot( state->currentSnapshot, state, nullptr );
        state->currentSnapshot = snapshot;

        bool compatiblesRemaining = ApplyObservedPatternAt( observedCellIndex, observedDistributionIndex, snapshot, state );
        result = compatiblesRemaining ? InProgress : Contradiction;
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
StartWFCSync( Job* job )
{
    const Spec& spec = *job->spec;
    State* state = job->state = PUSH_STRUCT( &job->memory->arena, State );

    // TODO We need to do most (or all!) of this in advance
    Init( spec, job->pOutputChunk, state, &job->memory->arena ); 

    while( !IsFinished( *state ) )
    {
        TIMED_BLOCK;

        if( job->cancellationRequested )
            state->currentResult = Cancelled;

        if( state->currentResult == InProgress )
            state->currentResult = Observe( spec, state, &job->memory->arena );

        if( state->currentResult == Contradiction )
        {
            state->contradictionCount++;
            state->currentResult = RewindSnapshot( state );
        }

        if( state->currentResult == InProgress )
            state->currentResult = Propagate( spec, state );
    }
}

inline Job*
BeginJobWithMemory( GlobalState* globalState )
{
    JobMemory* freeMemory = nullptr;
    for( u32 i = 0; i < globalState->jobMemoryChunks.count; ++i )
    {
        JobMemory* memory = &globalState->jobMemoryChunks[i];
        bool inUse = AtomicLoad( &memory->inUse );
        if( !inUse )
        {
            AtomicExchange( &memory->inUse, true );
            ClearArena( &memory->arena );
            freeMemory = memory;
            break;
        }
    }

    Job* result = nullptr;
    if( freeMemory )
    {
        for( u32 i = 0; i < globalState->jobs.count; ++i )
        {
            Job* job = &globalState->jobs[i];
            bool inUse = AtomicLoad( &job->inUse );
            if( !inUse )
            {
                AtomicExchange( &job->inUse, true );
                job->memory = freeMemory;
                result = job;
                break;
            }
        }
    }

    return result;
}

inline void
EndJobWithMemory( Job* job )
{
    AtomicExchange( &job->inUse, false );
    AtomicExchange( &job->memory->inUse, false );
}

internal
PLATFORM_JOBQUEUE_CALLBACK(DoWFC)
{
    Job* job = (Job*)userData;
    StartWFCSync( job );

    EndJobWithMemory( job );
}

internal bool
TryStartJobFor( const v2u& pOutputChunk, GlobalState* globalState )
{
    bool result = false;

    Job* job = BeginJobWithMemory( globalState );
    if( job )
    {
        job->spec = &globalState->spec;
        job->pOutputChunk = pOutputChunk;

        globalPlatform.AddNewJob( globalPlatform.hiPriorityQueue,
                                  DoWFC,
                                  job );

        globalState->outputChunks.At( pOutputChunk ).buildJob = job;
        result = true;
    }

    return result;
}

GlobalState*
StartWFCAsync( const Spec& spec, const v2u& pStartChunk, MemoryArena* wfcArena )
{
    u32 maxJobCount = 8;

    GlobalState* result = PUSH_STRUCT( wfcArena, GlobalState );
    *result =
    {
        spec,
        Array<Job>( wfcArena, maxJobCount, maxJobCount ),
        Array<JobMemory>( wfcArena, maxJobCount, maxJobCount ),
        Array2<ChunkInfo>( wfcArena, spec.outputChunkCount.x, spec.outputChunkCount.y ),
        wfcArena,
    };

    // Partition parent arena
    sz perJobArenaSize = Available( *wfcArena ) / maxJobCount;
    for( u32 i = 0; i < maxJobCount; ++i )
    {
        JobMemory& memory = result->jobMemoryChunks[i];
        memory.arena = MakeSubArena( wfcArena, perJobArenaSize );
        memory.inUse = false;
    }

    // Start first job
    if( TryStartJobFor( pStartChunk, result ) )
        ++result->processedChunkCount;
    else
        INVALID_CODE_PATH;

    return result;
}

Job*
UpdateWFCAsync( GlobalState* globalState )
{
    Job* result = nullptr;
    if( globalState->jobs )
    {
        while( globalState->processedChunkCount < globalState->outputChunks.Count() )
        {
            u32 nextIndex = globalState->processedChunkCount;
            u32 chunkCountX = globalState->spec.outputChunkCount.x;
            v2u pChunk = { nextIndex % chunkCountX, nextIndex / chunkCountX };

            if( TryStartJobFor( pChunk, globalState ) )
                ++globalState->processedChunkCount;
            else
                break;
        }
        result = &globalState->jobs[0];
    }

    return result;
}


// TODO Clarify inputs & outputs
internal void
CreateOutputTexture( const Spec& spec, const GlobalState* globalState, u32* imageBuffer, Texture* result )
{
    const u32 N = spec.N;
    const u32 width = spec.outputChunkDim.x;
    const u32 height = spec.outputChunkDim.y;
    const v2u totalOutputDim = Hadamard( spec.outputChunkDim, spec.outputChunkCount );
    const u32 pitch = totalOutputDim.x;

    for( u32 chunkY = 0; chunkY < globalState->outputChunks.rows; ++chunkY )
    {
        for( u32 chunkX = 0; chunkX < globalState->outputChunks.cols; ++chunkX )
        {
            v2u pChunk = { chunkX, chunkY };
            Job* job = globalState->outputChunks.At( pChunk ).buildJob;
            if( !job )
                continue;

            const v2u chunkCoords = Hadamard( spec.outputChunkDim, pChunk );
            u32* chunkOrig = imageBuffer + chunkCoords.y * pitch + chunkCoords.x;
            u32* out;

            Snapshot* snapshot = job->state->currentSnapshot;
#if 1
            if( job->state->currentResult == InProgress )
            {
                for( u32 y = 0; y < height; ++y )
                {
                    out = chunkOrig + y * pitch;
                    for( u32 x = 0; x < width; ++x )
                    {
                        int contributors = 0, r = 0, g = 0, b = 0;
                        u32 color = 0;

                        if( x != 0 && y != 0 )
                        {
                            for( u32 dy = 0; dy < N; ++dy )
                            {
                                int cellY = (int)(y - dy);
                                // FIXME Change this to account for chunk partitioning, etc
                                if( cellY < 0 )
                                    cellY += height;

                                for( u32 dx = 0; dx < N; ++dx )
                                {
                                    int cellX = (int)(x - dx);
                                    if( cellX < 0 )
                                        cellX += width;

                                    if( OnBoundary( { cellX, cellY }, spec ) )
                                        continue;

                                    for( u32 p = 0; p < job->state->patterns.count; ++p )
                                    {
                                        if( GetWaveAt( snapshot, cellY * width + cellX, p, job->state ) )
                                        {
                                            u32 cr, cg, cb;
                                            u8 sample = job->state->patterns[p].data[dy * N + dx];
                                            UnpackRGBA( job->state->palette[sample], &cr, &cg, &cb );
                                            r += cr;
                                            g += cg;
                                            b += cb;
                                            contributors++;
                                        }
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
            else 
#endif
            if( job->state->currentResult == Done )
            {
                for( u32 y = 0; y < height; ++y )
                {
                    out = chunkOrig + y * pitch;

                    u32 cellY = (y < height - N + 1) ? y : y - N + 1;
                    u32 dy = (y < height - N + 1) ? 0 : y + N - height;

                    for( u32 x = 0; x < width; ++x )
                    {
                        u32 cellX = (x < width - N + 1) ? x : x - N + 1;
                        u32 dx = (x < width - N + 1) ? 0 : x + N - width;

                        u32 color = 0;
                        if( x != 0 && y != 0 )
                        {
                            u32 patternIndex = U32MAX;
                            u32 cellIndex = cellY * width + cellX;
                            for( u32 p = 0; p < job->state->patterns.count; ++p )
                            {
                                if( GetWaveAt( snapshot, cellIndex, p, job->state ) )
                                {
                                    patternIndex = p;
                                    break;
                                }
                            }
                            ASSERT( patternIndex != U32MAX );

                            u8* patternSamples = job->state->patterns[patternIndex].data;
                            u8 sample = patternSamples[dy * N + dx];
                            color = job->state->palette[sample];
                        }

                        *out++ = color;
                    }
                }
            }
            else if( job->state->currentResult == Contradiction || job->state->currentResult == Failed )
            {
                for( u32 y = 0; y < height; ++y )
                {
                    out = chunkOrig + y * pitch;
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

        }
    }

    // Use existing handle if we have one
    result->handle = globalPlatform.AllocateOrUpdateTexture( imageBuffer, totalOutputDim.x, totalOutputDim.y, false,
                                                             result->handle );
    result->imageBuffer = imageBuffer;
    result->width = totalOutputDim.x;
    result->height = totalOutputDim.y;
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

internal u32
CountNonZero( const Array<r32>& distribution )
{
    u32 result = 0;
    for( u32 i = 0; i < distribution.count; ++i )
        if( distribution[i] != 0.f )
            result++;

    return result;
}

u32
DrawTest( const Array<Spec>& specs, const GlobalState* globalState, DisplayState* displayState,
          const v2& displayDim,
          const DebugState* debugState, MemoryArena* wfcDisplayArena, const TemporaryMemory& tmpMemory )
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

    // FIXME
    State* state = globalState->jobs[0].state;
    const u32 patternsCount = state ? state->patterns.count : 0;
    const u32 patternsCountSqrt = (u32)Round( Sqrt( (r32)patternsCount ) );
    const u32 waveLength = spec.outputChunkDim.x * spec.outputChunkDim.y;

    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( displayState->currentPage )
    {
        case TestPage::Output:
        {
            if( state )
            {
                ImGui::BeginChild( "child_output_meta", ImVec2( 350, 0 ) );

                if( state->currentResult == Done )
                    ImGui::Text( "DONE!" );
                else if( state->currentResult == Failed )
                    ImGui::Text( "FAILED!" );
                else
                    ImGui::Text( "Remaining: %d (%d)", waveLength - state->observationCount, state->observationCount );
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::Text( "Name: %s", spec.name );
                ImGui::Text( "Num. patterns: %d", patternsCount );
                ImGui::Spacing();
                ImGui::Spacing();

                if( state->currentSnapshot )
                {
                    ImGui::Text( "Wave size: %zu", state->currentSnapshot->wave.Size() );
                    ImGui::Text( "Adjacency size: %zu", state->currentSnapshot->adjacencyCounters.Size() );
                }
                if( state->arena )
                    ImGui::Text( "Total memory: %d / %d", state->arena->used / MEGABYTES(1), state->arena->size / MEGABYTES(1) );
                ImGui::Spacing();
                ImGui::Spacing();

                if( state->snapshotStack )
                {
                    ImGui::Text( "Snapshot stack: %d / %d", state->snapshotStack.count, state->snapshotStack.maxCount );
                    ImGui::Text( "Backtracked cells cache: %d", state->backtrackedCellIndices.Count() );
                    ImGui::Text( "Total contradictions: %d", state->contradictionCount );
                    ImGui::Spacing();
                    ImGui::Spacing();

                    const u32 maxLines = 50;
                    //r32 step = (r32)maxLines / state->snapshotStack.buffer.maxCount;
                    for( u32 i = 0; i < state->snapshotStack.count; ++i )
                    {
                        const Snapshot& s = state->snapshotStack[i];
                        if( &s == state->currentSnapshot )
                            ImGui::Text( "snapshot[%d] : current", i );
                        else
                            ImGui::Text( "snapshot[%d] : n %d, observations %d",
                                         i, CountNonZero( s.distribution ), s.lastObservationCount );
                    }
                }
                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild( "child_output_image", ImVec2( 1100, 0 ) );
                v2u imageSize = { displayState->outputTexture.width, displayState->outputTexture.height };

                if( state->currentResult != displayState->lastDisplayedResult ||
                    state->observationCount != displayState->lastTotalObservations )
                {
                    // NOTE Very Broken Synchronization, but I don't really want to synchronize..
                    displayState->lastDisplayedResult = state->currentResult;
                    displayState->lastTotalObservations = state->observationCount;

                    if( !displayState->outputImageBuffer )
                        displayState->outputImageBuffer = PUSH_ARRAY( wfcDisplayArena, imageSize.x * imageSize.y, u32 );

                    CreateOutputTexture( spec, globalState, displayState->outputImageBuffer, &displayState->outputTexture );
                }

                ImGui::Image( displayState->outputTexture.handle, imageSize * outputScale );

                if( ImGui::IsItemClicked() )
                    selectedIndex = displayState->currentSpecIndex;

                ImGui::EndChild();
                ImGui::SameLine();

                ImGui::BeginChild( "child_debug", ImVec2( 0, 0 ) );
                if( debugState )
                {
                    DrawPerformanceCounters( debugState, tmpMemory );
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
