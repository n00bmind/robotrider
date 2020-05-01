#if NON_UNITY_BUILD
#include "imgui/imgui.h"
#include "ui.h"
#include "renderer.h"
#include "wfc.h"
#include "platform.h"
#include "debugstats.h"
#endif

namespace WFC
{


internal void
PalettizeSource( const Texture& source, Input* input, MemoryArena* arena )
{
    input->inputSamples = PUSH_ARRAY( arena, u8, source.width * source.height );

    for( u32 i = 0; i < source.width * source.height; ++i )
    {
        u8 colorSample = 0;
        u32 color = *((u32*)source.imageBuffer + i);

        bool found = false;
        for( u32 c = 0; c < input->paletteEntryCount; ++c )
        {
            if( input->palette[c] == color )
            {
                colorSample = ToU8Safe( c );
                found = true;
                break;
            }
        }

        if( !found )
        {
            ASSERT( input->paletteEntryCount < ARRAYCOUNT(input->palette) );
            input->palette[input->paletteEntryCount] = color;
            colorSample = ToU8Safe( input->paletteEntryCount );
            ++input->paletteEntryCount;
        }

        *(input->inputSamples + i) = colorSample;
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
    // TODO Test! (could this be related to the obviously wrong frequency figures!?)
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
AddPatternWithData( u8* data, const u32 N, Input* input, MemoryArena* arena )
{
    Pattern testPattern = { data, N };

    u32* patternCountValue = input->patternsHash.Find( testPattern );
    if( patternCountValue )
        ++(*patternCountValue);
    else
    {
        // This is '<=' because we never count ourselves in each pattern adjacency count
        if (input->patternsHash.count <= MaxAdjacencyCount)
        {
            u8* patternData = PUSH_ARRAY( arena, u8, N * N );
            COPY(data, patternData, N * N);
            input->patternsHash.Insert({ patternData, N }, 1 );
        }
        else
            LOG("WARN :: Truncating input patterns at %d!", MaxAdjacencyCount);
    }
}

internal void
BuildPatternsFromSource( const u32 N, const v2u& inputDim, Input* input, MemoryArena* arena )
{
    u8* patternData = PUSH_ARRAY( arena, u8, N * N );
    u8* rotatedPatternData = PUSH_ARRAY( arena, u8, N * N );
    u8* reflectedPatternData = PUSH_ARRAY( arena, u8, N * N );

    // NOTE Totally arbitrary number for the entry count
    new (&input->patternsHash) HashTable<Pattern, u32, PatternHash>( arena, inputDim.x * inputDim.y / 2 );

    // NOTE We assume 'periodicInput' to be true
    for( u32 j = 0; j < (u32)inputDim.y; ++j )
    {
        for( u32 i = 0; i < (u32)inputDim.x; ++i )
        {
            ExtractPattern( input->inputSamples, i, j, inputDim, N, patternData );
            AddPatternWithData( patternData, N, input, arena );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, input, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, input, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, input, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, input, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, input, arena );
            RotatePattern( patternData, N, rotatedPatternData );
            AddPatternWithData( rotatedPatternData, N, input, arena );
            COPY( rotatedPatternData, patternData, N * N );

            ReflectPattern( patternData, N, reflectedPatternData );
            AddPatternWithData( reflectedPatternData, N, input, arena );
        }
    }

    // TODO Pack patterns
    input->patterns = input->patternsHash.Keys( arena );
}

internal Texture
CreatePatternTexture( const Pattern& pattern, const u32* palette, MemoryArena* arena )
{
    u32 len = pattern.stride * pattern.stride;
    u32* imageBuffer = PUSH_ARRAY( arena, u32, len );
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
BuildPatternsIndex( const u32 N, Input* input, MemoryArena* arena )
{
    u32 patternCount = input->patterns.count;

    for( u32 d = 0; d < ARRAYCOUNT(input->patternsIndex); ++d )
    {
        Array<IndexEntry> entries( arena, 0, patternCount );

        for( u32 p = 0; p < patternCount; ++p )
        {
            u8* pData = input->patterns[p].data;

            Array<bool> matches( arena, 0, patternCount );

            for( u32 q = 0; q < patternCount; ++q )
            {
                u8* qData = input->patterns[q].data;
                matches.Push( Matches( pData, AdjacencyMeta[d].cellDelta.xy, qData, N ) );
            }
            entries.Push( { matches } );
        }
        input->patternsIndex[d] = { entries };
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

inline void
SetAdjacencyAt( u32 cellIndex, u32 patternIndex, u32 dirIndex, Snapshot* snapshot, u32 adjacencyCount )
{
    u64 exp = dirIndex * BitsPerAxis;
    u64 value = (u64)adjacencyCount << exp;
    u64 mask = AdjacencyMeta[dirIndex].counterMask;

    u64& counter = snapshot->adjacencyCounters.AtRowCol( cellIndex, patternIndex );

    counter = (counter & ~mask) | (value & mask);
}

inline bool
DecreaseAdjacencyAndTestZeroAt( u32 cellIndex, u32 patternIndex, u32 dirIndex, Snapshot* snapshot )
{
    TIMED_BLOCK;

    u64 exp = dirIndex * BitsPerAxis;
    u64 one = 1ULL << exp;
    u64 mask = AdjacencyMeta[dirIndex].counterMask;

    u64& counter = snapshot->adjacencyCounters.AtRowCol( cellIndex, patternIndex );

    // NOTE This has been seen to underflow (trying to prevent that actually seems to break things!) (maybe test this again?)
    u64 value = (counter & mask) - one;
    counter = (counter & ~mask) | (value & mask);

    return value == 0;
}

inline bool
GetWaveAt( const Array2<u64>& wave, u32 cellIndex, u32 patternIndex )
{
    u32 patternSubIndex = patternIndex / 64;
    u64 bitset = wave.AtRowCol( cellIndex, patternSubIndex );

    u32 bitIndex = patternIndex % 64;
    u64 mask = 1ULL << bitIndex;

    return (bitset & mask) != 0;
}

inline void
CollapseWaveAt( Array2<u64>* wave, u32 cellIndex, u32 patternIndex, u32 totalPatternCount )
{
    u64* row = wave->AtRow( cellIndex );
    u32 patternGroupIndex = patternIndex / 64;

    u32 groupCount = totalPatternCount / 64 + 1;
    for( u32 p = 0; p < groupCount; ++p )
    {
        if( p == patternGroupIndex )
        {
            u32 bitIndex = patternIndex % 64;
            row[p] = (1ULL << bitIndex);
        }
        else
            row[p] = 0ULL;
    }
}

inline void
DisableWaveAt( Array2<u64>* wave, u32 cellIndex, u32 patternIndex )
{
    u32 patternGroupIndex = patternIndex / 64;
    u64& bitset = wave->AtRowCol( cellIndex, patternGroupIndex );

    u32 bitIndex = patternIndex % 64;
    u64 mask = ~(1ULL << bitIndex);

    bitset = bitset & mask;
}

inline void
ResetWaveAt( Array2<u64>* wave, u32 cellIndex, u32 totalPatternCount )
{
    u64* row = wave->AtRow( cellIndex );

    u32 groupCount = totalPatternCount / 64;
    for( u32 p = 0; p < groupCount; ++p )
        row[p] = ~0ULL;

    u32 spareCount = totalPatternCount % 64;
    if( spareCount )
        row[groupCount] = ((~0ULL) >> (64 - spareCount));
}

internal bool
BanPatternAtCell( u32 cellIndex, u32 patternIndex, State* state, const Input& input )
{
    TIMED_BLOCK;

    bool result = true;
    Snapshot* snapshot = state->currentSnapshot;

    if( GetWaveAt( snapshot->wave, cellIndex, patternIndex ) )
    {
        DisableWaveAt( &snapshot->wave, cellIndex, patternIndex );
        snapshot->adjacencyCounters.AtRowCol( cellIndex, patternIndex ) = 0;

        snapshot->compatiblesCount[cellIndex]--;
        if( snapshot->compatiblesCount[cellIndex] == 0 )
            result = false;

        // Weird entropy calculation
        u32 i = cellIndex;
        snapshot->entropies[i] += snapshot->sumWeights[i] / snapshot->sumFrequencies[i] - Log( snapshot->sumFrequencies[i] );

        snapshot->sumFrequencies[i] -= input.frequencies[patternIndex];
        snapshot->sumWeights[i] -= input.weights[patternIndex];

        snapshot->entropies[i] -= snapshot->sumWeights[i] / snapshot->sumFrequencies[i] - Log( snapshot->sumFrequencies[i] );

        // Add entry to stack
        if( result )
            state->propagationStack.Push( { cellIndex, patternIndex } );
    }

    return result;
}

inline bool
PropagateTo( u32 adjacentIndex, u32 d, u32 bannedPatternIndex, const Input& input, State* state )
{
    bool result = true;

    u32 oppositeIndex = AdjacencyMeta[d].oppositeIndex;
    // Get the list of patterns that match the banned pattern in adjacent direction d
    const Array<bool>& matches = input.patternsIndex[d].entries[bannedPatternIndex].matches;

    for( u32 p = 0; p < input.patterns.count; ++p )
    {
        if( matches[p] )
        {
            // For every match, decrease the adjacency count for the opposite direction of the corresponding pattern in the adjacent neighbour
            bool isZero = DecreaseAdjacencyAndTestZeroAt( adjacentIndex, p, oppositeIndex, state->currentSnapshot );
            if( isZero )
            {
                bool compatiblesRemaining = BanPatternAtCell( adjacentIndex, p, state, input );
                if( !compatiblesRemaining )
                {
                    result = false;
                    break;
                }
            }
        }
    }

    return result;
}

inline v3u
GetWaveDim( const Spec& spec )
{
    // NOTE Even though we don't need the last N-1 cells to construct each chunk's complete output,
    // we do need them to maintain proper continuity among adjacent chunks

    // Augment the wave so we have a border on every side (for proper inter-chunk propagation)
    v3u result = spec.outputChunkDim + V3u( 2 * spec.safeMarginWidth );
    return result;
}

// NOTE These will have to change substantially for 3D!
inline u32 
GetCellCountForAdjacency( u32 adjacencyIndex, const Spec& spec )
{
    u32 result = 0;
    v3u waveDim = GetWaveDim( spec );

    switch( adjacencyIndex )
    {
        case Adjacency::Left:
        case Adjacency::Right:
        {
            result = waveDim.y - 2 * spec.safeMarginWidth;
        } break;

        case Adjacency::Top:
        case Adjacency::Bottom:
        {
            result = waveDim.x - 2 * spec.safeMarginWidth;
        } break;

        INVALID_DEFAULT_CASE;
    }

    return result;
}

inline u32 
GetCellIndexAtAdjacencyBorder( u32 adjacencyIndex, u32 borderCellCounter, const Spec& spec, u32 fromBorder )
{
    u32 cellX = 0, cellY = 0;
    v3u waveDim = GetWaveDim( spec );
    u32 margin = spec.safeMarginWidth;

    switch( adjacencyIndex )
    {
        case Adjacency::Left:
        {
            cellX = fromBorder;
            cellY = margin + borderCellCounter;
        } break;

        case Adjacency::Right:
        {
            cellX = waveDim.x - (fromBorder + 1);
            cellY = margin + borderCellCounter;
        } break;

        case Adjacency::Top:
        {
            cellX = margin + borderCellCounter;
            cellY = fromBorder;
        } break;

        case Adjacency::Bottom:
        {
            cellX = margin + borderCellCounter;
            cellY = waveDim.y - (fromBorder + 1);
        } break;

        INVALID_DEFAULT_CASE;
    }

    ASSERT( cellX >= 0 && cellX < waveDim.x );
    ASSERT( cellY >= 0 && cellY < waveDim.y );
    u32 result = cellY * waveDim.x + cellX;
    return result;
}

internal bool
ApplyObservedPatternAt( u32 observedCellIndex, u32 observedPatternIndex, State* state, const Input& input )
{
    TIMED_BLOCK;

    Snapshot* snapshot = state->currentSnapshot;
    bool compatiblesRemaining = true;

    for( u32 p = 0; p < input.patterns.count; ++p )
    {
        if( GetWaveAt( snapshot->wave, observedCellIndex, p ) && p != observedPatternIndex )
        {
            compatiblesRemaining = BanPatternAtCell( observedCellIndex, p, state, input );
            if( !compatiblesRemaining )
                break;
        }
    }

    return compatiblesRemaining;
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

internal Result
Propagate( const Spec& spec, const Input& input, State* state )
{
    Result result = InProgress;

    v3u waveDim = GetWaveDim( spec );
    u32 width = waveDim.x;
    u32 height = waveDim.y;

    while( result == InProgress && state->propagationStack.count > 0 )
    {
        TIMED_BLOCK;

        BannedTuple ban = state->propagationStack.Pop();
        v2i pCell = V2i( PositionFromIndex( ban.cellIndex, width ) );

        for( u32 d = 0; d < Adjacency::Count; ++d )
        {
            v2i pAdjacent = pCell + AdjacencyMeta[d].cellDelta.xy;

            if( !spec.periodic && (pAdjacent.x < 0 || pAdjacent.y < 0 || pAdjacent.x >= (i32)width || pAdjacent.y >= (i32)height) )
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
            if( !PropagateTo( adjacentIndex, d, ban.patternIndex, input, state ) )
            {
                result = Contradiction;
                break;
            }
        }
    }

    return result;
}

internal bool
Init( const Spec& spec, const Input& input, const ChunkInitInfo& initInfo, State* state, MemoryArena* arena )
{
    TIMED_BLOCK;

    bool result = false;
    state->arena = arena;

    v3u waveDim = GetWaveDim( spec );
    const u32 waveLength = waveDim.x * waveDim.y;
    const u32 patternCount = input.patterns.count;
    const u32 propagationStackSize = waveLength * 5;                            // Increase as needed
    state->propagationStack = Array<BannedTuple>( arena, 0, propagationStackSize );
    state->distributionTemp = Array<r32>( arena, patternCount, patternCount );
    state->backtrackedCellIndices = RingBuffer<u32>( arena, BacktrackedCellsCacheCount );

    // Now we want to calculate the max number of snapshots that will fit in our arena,
    // but we still need to allocate the snapshot stack, whose size depends on the number of snapshots
    // Now, given:
    sz S = initInfo.snapshot0Size;                                              // Size in memory for all the snapshot's data
    sz A = arena->size - arena->used;                                           // Total available for all snapshots + stack
    sz s = sizeof(Snapshot);                                                    // Size of the snapshot struct itself
    // We find that:
    // The size of the snapshot stack content (barring the stack struct itself) would be x = n * s, where n is the number of snapshots
    // Conversely, n = (A - x) / S
    // So if we substitute x and solve for n we have:
    u32 maxSnapshotCount = ToU32Safe( (r32)A / (S + s) );
    ASSERT( maxSnapshotCount * s + maxSnapshotCount * S < A );

    // Truncate at 64, since having more does actually seem less effective
    maxSnapshotCount = Min( maxSnapshotCount, MaxBacktrackingDepth );

    // @Incomplete Calc total arena waste here and do something about it!

    // We need to allocate the stack before any snapshot, so that rewinding snapshots will work properly
    state->snapshotStack = Array<Snapshot>( arena, 0, maxSnapshotCount );

    // Create initial snapshot
    Snapshot snapshot0;
    AllocSnapshot( &snapshot0, waveLength, patternCount, arena );
    CopySnapshot( initInfo.snapshot0, &snapshot0 );
    state->currentSnapshot = state->snapshotStack.Push( snapshot0 );

    // Propagate results from adjacent chunks
    // TODO Corners  !!!!!
    for( u32 d = 0; d < Adjacency::Count; ++d )
    {
        if( initInfo.adjacentChunks[d] )
        {
            //result = true;
            u32 dOpp = AdjacencyMeta[d].oppositeIndex;

            u32 cellCount = GetCellCountForAdjacency( d, spec );

            for( u32 r = 0; r < spec.safeMarginWidth; ++r )
            {
                for( u32 i = 0; i < cellCount; ++i )
                {
                    u32 currentChunkCellIndex = GetCellIndexAtAdjacencyBorder( d, i, spec, r );
                    u32 adjacentChunkCellIndex = GetCellIndexAtAdjacencyBorder( dOpp, i, spec, 2 * spec.safeMarginWidth - r - 1 );

                    u32 collapsedPatternIndex = (*initInfo.adjacentChunks[d])[ adjacentChunkCellIndex ];

                    // Observe
                    {
                        bool compatiblesRemaining = ApplyObservedPatternAt( currentChunkCellIndex, collapsedPatternIndex, state, input );
                        // FIXME How do we stop getting these
                        //ASSERT( compatiblesRemaining );
                        // Fake it till you make it
                        CollapseWaveAt( &state->currentSnapshot->wave, currentChunkCellIndex, collapsedPatternIndex, patternCount );
                        state->currentSnapshot->compatiblesCount[currentChunkCellIndex] = 1;

                        state->observationCount++;
                    }

                    // Propagate
                    {
                        state->currentResult = Propagate( spec, input, state );
                        ASSERT( state->currentResult == InProgress );
                    }
                }
            }
        }
    }

    state->currentResult = InProgress;
    return result;
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
BuildDistributionAndSelectAt( u32 cellIndex, Snapshot* snapshot, State* state, u32* selection, const Input& input )
{
    for( u32 p = 0; p < input.patterns.count; ++p )
        snapshot->distribution[p] = (GetWaveAt( snapshot->wave, cellIndex, p ) ? input.frequencies[p] : 0.f);

    bool result = RandomSelect( snapshot->distribution, &state->distributionTemp, selection );
    return result;
}

// NOTE If the snapshot space was previously allocated, uses that same space, otherwise allocates
internal Snapshot*
PushNewSnapshot( Snapshot* source, State* state, u32 patternCount, MemoryArena* arena )
{
    Snapshot* result = state->snapshotStack.PushEmpty( false );

    if( !result->wave )
        AllocSnapshot( result, source->wave.rows, patternCount, arena );
    CopySnapshot( source, result );

    result->lastObservedDistributionIndex = 0;
    result->lastObservedCellIndex = 0;
    result->lastObservationCount = 0;

    return result;
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
Observe( const Spec& spec, const Input& input, State* state, MemoryArena* arena )
{
    TIMED_BLOCK;

    Result result = InProgress;
    Snapshot* snapshot = state->currentSnapshot;

    u32 observedCellIndex = 0;
    r64 minEntropy = R64INF;

    u32 observations = 0;
    for( u32 i = 0; i < snapshot->wave.rows; ++i )
    {
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
            u32 observedDistributionIndex;
            bool haveSelection = BuildDistributionAndSelectAt( observedCellIndex, snapshot, state,
                                                               &observedDistributionIndex, input );
            ASSERT( haveSelection );

            if( NeedSnapshot( state, snapshot->wave.rows ) )
            {
                snapshot->lastObservedCellIndex = observedCellIndex;
                snapshot->lastObservedDistributionIndex = observedDistributionIndex;
                snapshot->lastObservationCount = state->observationCount;

                snapshot = PushNewSnapshot( state->currentSnapshot, state, input.patterns.count, arena );
                state->currentSnapshot = snapshot;
            }

            bool compatiblesRemaining = ApplyObservedPatternAt( observedCellIndex, observedDistributionIndex, state, input );
            if( !compatiblesRemaining )
                result = Contradiction;
        }
    }

    return result;
}

internal Result
RewindSnapshot( State* state, const Input& input )
{
    TIMED_BLOCK;

    Result result = Contradiction;
    Snapshot* snapshot = state->currentSnapshot;

    state->propagationStack.Clear();

    bool haveNewSelection = false;
    while( snapshot && !haveNewSelection )
    {
        // Get previous snapshot from the stack, if available
        state->snapshotStack.Pop();
        if( state->snapshotStack.count )
        {
            snapshot = &state->snapshotStack.Last();
            // Discard the random selection we chose last time, and try a different one
            snapshot->distribution[snapshot->lastObservedDistributionIndex] = 0.f;
            haveNewSelection = RandomSelect( snapshot->distribution, &state->distributionTemp, &snapshot->lastObservedDistributionIndex );

            if( !haveNewSelection )
            {
                if( BacktrackedCellsCacheCount )
                {
                    // Discard this cell index entirely and try a different one
                    // Do we somehow want to retry distribution options once we discard new cells?
                    state->backtrackedCellIndices.Push( snapshot->lastObservedCellIndex );
                }
#if 0
                Sleep( 250 );
#endif
            }
        }
        else
            break;
    }

    if( haveNewSelection )
    {
        state->currentSnapshot = snapshot;

        u32 observedCellIndex = snapshot->lastObservedCellIndex;
        u32 observedDistributionIndex = snapshot->lastObservedDistributionIndex;

        // NOTE We know we have backtracked at least once, so the storage for the snapshot is guaranteed to be allocated
        snapshot = PushNewSnapshot( state->currentSnapshot, state, input.patterns.count, nullptr );
        state->currentSnapshot = snapshot;

        bool compatiblesRemaining = ApplyObservedPatternAt( observedCellIndex, observedDistributionIndex, state, input );
        result = compatiblesRemaining ? InProgress : Contradiction;
    }
    else
        result = Failed;

    return result;
}

internal void
ExtractOutputFromWave( const Array2<u64>& wave, const Spec& spec, const Input& input, Array<u32>* collapsedWave, Array2<u8>* outputSamples )
{
    u8* out = outputSamples->data;
    u32* waveOut = collapsedWave->data;
    v3u waveDim = GetWaveDim( spec );
    u32 margin = spec.safeMarginWidth;

    for( u32 y = 0; y < waveDim.y; ++y )
    {
        for( u32 x = 0; x < waveDim.x; ++x )
        {
            u32 patternIndex = U32MAX;
            u32 cellIndex = y * waveDim.x + x;
            // @Speed We could do this faster with bit testing
            for( u32 p = 0; p < input.patterns.count; ++p )
            {
                if( GetWaveAt( wave, cellIndex, p ) )
                {
                    patternIndex = p;
                    break;
                }
            }
            ASSERT( patternIndex != U32MAX );

            waveOut[cellIndex] = patternIndex;

            // Account for the border when tiling chunks
            if( x >= margin && y >= margin && x < (waveDim.x - margin) && y < (waveDim.y - margin) )
            {
                u8* patternSamples = input.patterns[patternIndex].data;
                *out++ = patternSamples[0];
            }
        }
    }
}

internal bool
IsFinished( const State& state )
{
    return state.currentResult >= Cancelled;
}

Result
DoWFCSync( Job* job )
{
    const Spec& spec = *job->spec;
    const Input& input = *job->input;
    State* state = job->state = PUSH_STRUCT( &job->memory->arena, State );

    bool propagateFirst = Init( spec, input, job->initInfo, state, &job->memory->arena ); 

    while( !IsFinished( *state ) )
    {
        TIMED_BLOCK;

        if( job->cancellationRequested )
            state->currentResult = Cancelled;

        if( !propagateFirst )
        {
            if( state->currentResult == InProgress )
                state->currentResult = Observe( spec, input, state, &job->memory->arena );

            if( state->currentResult == Contradiction )
            {
                state->contradictionCount++;
                state->currentResult = RewindSnapshot( state, input );
            }

            // We currently just restart the job instead
#if 0
            if( state->currentResult == Failed )
            {
                ClearArena( &job->memory->arena );
                state = job->state = PUSH_STRUCT( &job->memory->arena, State );
                // @Speed Could save some time here?
                Init( spec, input, job->initInfo, state, &job->memory->arena ); 
                continue;
            }
#endif
        }

        if( state->currentResult == InProgress )
            state->currentResult = Propagate( spec, input, state );

        propagateFirst = false;
    }

    if( state->currentResult == Done )
        ExtractOutputFromWave( state->currentSnapshot->wave, spec, input, &job->outputChunk->collapsedWave, &job->outputChunk->outputSamples );
    return state->currentResult;
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
EndJobWithMemory( Job* job, Result result )
{
    AtomicExchange( (volatile u32*)&job->outputChunk->result, result );
    AtomicExchange( &job->inUse, false );
    AtomicExchange( &job->memory->inUse, false );
}

internal
PLATFORM_JOBQUEUE_CALLBACK(DoWFC)
{
    Job* job = (Job*)userData;
    Result result = DoWFCSync( job );

    EndJobWithMemory( job, result );
}

internal bool
TryStartJobFor( const v2u& pOutputChunk, GlobalState* globalState, const ChunkInitInfo& initInfo )
{
    bool result = false;

    Job* job = BeginJobWithMemory( globalState );
    if( job )
    {
        ChunkInfo* chunk = &globalState->outputChunks.At( pOutputChunk );
        chunk->buildJob = job;
        AtomicExchange( (volatile u32*)&chunk->result, InProgress );

        job->spec = &globalState->spec;
        job->input = &globalState->input;
        job->outputChunk = chunk;
        job->initInfo = initInfo;

        globalPlatform.AddNewJob( globalPlatform.hiPriorityQueue,
                                  DoWFC,
                                  job );

        result = true;
    }

    return result;
}

GlobalState* StartWFCAsync( const Spec& spec, const v2u& pStartChunk, MemoryArena* wfcArena )
{
    u32 maxJobCount = 8;

    GlobalState* result = PUSH_STRUCT( wfcArena, GlobalState );
    result->spec = spec;
    result->jobs = Array<Job>( wfcArena, maxJobCount, maxJobCount );
    result->jobMemoryChunks = Array<JobMemory>( wfcArena, maxJobCount, maxJobCount );
    result->outputChunks = Array2<ChunkInfo>( wfcArena, spec.outputChunkCount.y, spec.outputChunkCount.x );
    result->globalWFCArena = wfcArena;

    // Process global input data
    // TODO This probably should be done either offline or on a thread
    Input& input = result->input;
    {
        PalettizeSource( spec.source, &input, wfcArena );
        BuildPatternsFromSource( spec.N, { spec.source.width, spec.source.height }, &input, wfcArena );
        BuildPatternsIndex( spec.N, &input, wfcArena );
        u32 patternCount = input.patterns.count;

        input.frequencies = input.patternsHash.Values( wfcArena );
        ASSERT( input.frequencies.count == patternCount );
        input.weights = Array<r64>( wfcArena, patternCount, patternCount );
        // Calc initial entropy, counters, etc.
        input.sumFrequencies = 0;                                                     // sumOfWeights
        input.sumWeights = 0;                                                         // sumOfWeightLogWeights
        for( u32 p = 0; p < patternCount; ++p )
        {
            input.weights[p] = input.frequencies[p] * Log( input.frequencies[p] );
            input.sumFrequencies += input.frequencies[p];
            input.sumWeights += input.weights[p];
        }
        input.initialEntropy = Log( input.sumFrequencies ) - input.sumWeights / input.sumFrequencies;
    }

    v3u waveDim = GetWaveDim( spec );
    u32 waveLength = waveDim.x * waveDim.y; // * waveDim.z;

    // Init chunk info
    for( u32 i = 0; i < result->outputChunks.Count(); ++i )
    {
        ChunkInfo& info = result->outputChunks[i];
        info = {};
        info.collapsedWave = Array<u32>( wfcArena, waveLength, waveLength );
        info.outputSamples = Array2<u8>( wfcArena, spec.outputChunkDim.y, spec.outputChunkDim.x );
        info.pChunk = result->outputChunks.XYForIndex( i );
    }

    // Set up starting snapshot
    sz lowWaterMark = wfcArena->used;
    AllocSnapshot( &result->snapshot0, waveLength, input.patterns.count, wfcArena );
    result->snapshot0Size = wfcArena->used - lowWaterMark;

    {
        Snapshot& snapshot0 = result->snapshot0;

        // Set initial entropy and counters
        for( u32 i = 0; i < waveLength; ++i )
        {
            ResetWaveAt( &snapshot0.wave, i, input.patterns.count );
            snapshot0.compatiblesCount[i] = input.patterns.count;
            snapshot0.sumFrequencies[i] = input.sumFrequencies;
            snapshot0.sumWeights[i] = input.sumWeights;
            snapshot0.entropies[i] = input.initialEntropy;

            for( u32 p = 0; p < input.patterns.count; ++p )
            {
                for( u32 d = 0; d < Adjacency::Count; ++d )
                {
                    // Get the list of patterns that match p in the adjacent direction d
                    const Array<bool>& matches = input.patternsIndex[d].entries[p].matches;

                    u32 matchCount = 0;
                    for( u32 m = 0; m < matches.count; ++m )
                        if( matches[m] )
                            matchCount++;

                    // Set how many matches we initially have in that direction
                    SetAdjacencyAt( i, p, d, &snapshot0, matchCount );
                }
            }
        }
    }

    // Partition remaining space in parent arena
    sz perJobArenaSize = Available( *wfcArena ) / maxJobCount;
    for( u32 i = 0; i < maxJobCount; ++i )
    {
        JobMemory& memory = result->jobMemoryChunks[i];
        memory.arena = MakeSubArena( wfcArena, perJobArenaSize );
        memory.inUse = false;
    }

    return result;
}

inline ChunkInfo*
GetChunkForAdjacencyIndex( const v3i& pSourceChunk, u32 adjacencyIndex, GlobalState* globalState )
{
    ChunkInfo* result = nullptr;

    v3i pAdjacent = pSourceChunk + AdjacencyMeta[adjacencyIndex].cellDelta;
    i32 width = (i32)globalState->outputChunks.cols;
    i32 height = (i32)globalState->outputChunks.rows;
    // TODO 
    i32 depth = (i32)I32MAX;

    if( pAdjacent.x >= 0 && pAdjacent.y >= 0 && pAdjacent.z >= 0 &&
        pAdjacent.x < width && pAdjacent.y < height && pAdjacent.z < depth )
    {
        result = &globalState->outputChunks.At( V2u( pAdjacent.xy ) );
    }

    return result;
}

void UpdateWFCAsync( GlobalState* globalState )
{
    if( globalState->cancellationRequested )
    {
        if( !globalState->done )
        {
            u32 cancelledCount = 0;
            for( u32 i = 0; i < globalState->jobs.count; ++i )
            {
                Job* job = &globalState->jobs[i];
                AtomicExchange( &job->cancellationRequested, true );
                if( !AtomicLoad( &job->inUse ) )
                    ++cancelledCount;
            }
            if( cancelledCount == globalState->jobs.count )
                globalState->done = true;
        }
    }
    else
    {
        if( globalState->processedChunkCount < globalState->outputChunks.Count() )
        {
            i32 width = (i32)globalState->outputChunks.cols;
            i32 height = (i32)globalState->outputChunks.rows;
            // TODO 
            i32 depth = (i32)I32MAX;

            for( i32 y = 0; y < height; ++y )
            {
                for( i32 x = 0; x < width; ++x )
                {
                    ChunkInfo* chunk = &globalState->outputChunks.AtRowCol( y, x );
                    v3i pChunk = { x, y, 0 };

                    Result currentResult = (Result)AtomicLoad( (volatile u32*)&chunk->result );
                    switch( currentResult )
                    {
                        case NotStarted:
                        {
                            u32 validCount = 0;
                            u32 adjacentCount = Adjacency::Count;

                            // Gather adjacent chunk outputs as we go
                            ChunkInitInfo initInfo = {};
                            initInfo.snapshot0 = &globalState->snapshot0;
                            initInfo.snapshot0Size = globalState->snapshot0Size;

#if 1
                            bool canProceed = chunk->canProceed || (x == 0 && y == 0);
                            if( !canProceed )
                                continue;
#endif
                            // Start processing if no adjacent is currently in progress
                            // TODO Make everything faster by removing the 'canProceed' concept and instead extending this loop
                            // to also account for the diagonal neighbours
                            for( u32 i = 0; i < Adjacency::Count; ++i )
                            {
                                v3i pAdjacent = pChunk + AdjacencyMeta[i].cellDelta;

                                if( pAdjacent.x < 0 || pAdjacent.y < 0 || pAdjacent.z < 0 ||
                                    pAdjacent.x >= width || pAdjacent.y >= height || pAdjacent.z >= depth )
                                {
                                    adjacentCount--;
                                    continue;
                                }

                                // For now just check if in progress
                                ChunkInfo* adjacentChunk = &globalState->outputChunks.At( V2u( pAdjacent.xy ) );
                                Result adjacentResult = (Result)AtomicLoad( (volatile u32*)&adjacentChunk->result );
                                if( adjacentResult == NotStarted || adjacentResult == Done )
                                    validCount++;

                                if( adjacentResult == Done )
                                    initInfo.adjacentChunks[i] = &adjacentChunk->collapsedWave;
                            }

                            if( validCount == adjacentCount )
                            {
                                TryStartJobFor( V2u( pChunk.xy ), globalState, initInfo );
                            }
                        } break;

                        case Failed:
                        {
                            AtomicExchange( (volatile u32*)&chunk->result, NotStarted );
                        } break;

                        case Done:
                        {
                            if( !chunk->done )
                            {
                                chunk->done = true;
                                ++globalState->processedChunkCount;
#if 1
                                for( u32 d = 0; d < Adjacency::Count; ++d )
                                {
                                    ChunkInfo* adjacentChunk = GetChunkForAdjacencyIndex( pChunk, d, globalState );
                                    if( adjacentChunk )
                                        adjacentChunk->canProceed = true;
                                }
#endif
                            }
                        } break;
                    }
                }
            }
        }
    }
}


// TODO Clarify inputs & outputs
internal void
CreateOutputTexture( const Spec& spec, const GlobalState* globalState, u32* imageBuffer, Texture* result )
{
    const u32 N = spec.N;
    const Input& input = globalState->input;

    v3u waveDim = GetWaveDim( spec );
    const u32 waveWidth = waveDim.x;
    const u32 waveHeight = waveDim.y;
    const u32 chunkWidth = spec.outputChunkDim.x;
    const u32 chunkHeight = spec.outputChunkDim.y;

    const v3u totalOutputDim = Hadamard( spec.outputChunkDim, spec.outputChunkCount );
    const u32 totalPitch = totalOutputDim.x;

    for( u32 chunkY = 0; chunkY < globalState->outputChunks.rows; ++chunkY )
    {
        for( u32 chunkX = 0; chunkX < globalState->outputChunks.cols; ++chunkX )
        {
            v3u pChunk = { chunkX, chunkY, 0 };
            const ChunkInfo& chunkInfo = globalState->outputChunks.At( pChunk.xy );
            const v3u chunkCoords = Hadamard( spec.outputChunkDim, pChunk );

            u32* chunkOrig = imageBuffer + chunkCoords.y * totalPitch + chunkCoords.x;
            u32* out;

            if( chunkInfo.done )
            {
                bool failed = chunkInfo.result == Failed;
                for( u32 y = 0; y < chunkInfo.outputSamples.rows; ++y )
                {
                    out = chunkOrig + y * totalPitch;

                    for( u32 x = 0; x < chunkInfo.outputSamples.cols; ++x )
                    {
                        u8 sample = chunkInfo.outputSamples[y * chunkWidth + x];
                        u32 color = input.palette[sample];

                        if( failed ) // || x == 0 || y == 0 )
                        {
                            v4 unpacked = UnpackRGBAToV401( color );
                            unpacked = Hadamard( unpacked, { .5f, .5f, .5f, 1.f } );
                            color = Pack01ToRGBA( unpacked );
                        }

                        *out++ = color;
                    }
                }
            }
#if 1
            else
            {
                Job* job = chunkInfo.buildJob;
                if( !job || !job->state )
                    continue;
                Snapshot* snapshot = job->state->currentSnapshot;
                if( job->state->currentResult == InProgress )
                {
                    for( u32 y = 0; y < chunkHeight; ++y )
                    {
                        out = chunkOrig + y * totalPitch;
                        for( u32 x = 0; x < chunkWidth; ++x )
                        {
                            int contributors = 0, r = 0, g = 0, b = 0;
                            u32 color = 0;

                            for( u32 dy = 0; dy < N; ++dy )
                            {
                                int cellY = (int)(y + spec.safeMarginWidth - dy);

                                // @Correctness This wrap-around (and the one in X) should only happen for periodic specs
                                if( cellY < 0 )
                                    cellY += waveHeight;

                                for( u32 dx = 0; dx < N; ++dx )
                                {
                                    int cellX = (int)(x + spec.safeMarginWidth - dx);

                                    if( cellX < 0 )
                                        cellX += waveWidth;

                                    if( cellX < 0 || cellY < 0 )
                                        continue;

                                    for( u32 p = 0; p < input.patterns.count; ++p )
                                    {
                                        if( GetWaveAt( snapshot->wave, cellY * waveWidth + cellX, p ) )
                                        {
                                            u32 cr, cg, cb;
                                            u8 sample = input.patterns[p].data[dy * N + dx];
                                            UnpackRGBA( input.palette[sample], &cr, &cg, &cb );

                                            r += cr;
                                            g += cg;
                                            b += cb;
                                            contributors++;
                                        }
                                    }
                                }
                            }

                            if( contributors )
                            {
                                color = PackRGBA( r / contributors, g / contributors, b / contributors, 0xFF );
#if 1
                                if( x == 0 || y == 0 )
                                {
                                    v4 unpacked = UnpackRGBAToV401( color );
                                    unpacked = Hadamard( unpacked, { .5f, .5f, .5f, 1.f } );
                                    color = Pack01ToRGBA( unpacked );
                                }
#endif
                            }

                            *out++ = color;
                        }
                    }
                }
            }
#endif
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

internal bool
JobsInProgress( const GlobalState* globalState )
{
    bool result = false;

    for( u32 i = 0; i < globalState->jobs.count; ++i )
    {
        if( globalState->jobs[i].inUse )
        {
            result = true;
            break;
        }
    }

    return result;
}

u32 DrawTest( const Array<Spec>& specs, const GlobalState* globalState, DisplayState* displayState,
              const v2& pDisplay, const v2& displayDim, const DebugState* debugState, MemoryArena* wfcDisplayArena,
              const TemporaryMemory& tmpMemory )
{
    u32 selectedIndex = U32MAX;
    const Spec& spec = specs[displayState->currentSpecIndex];

    const r32 outputScale = 4.f;
    const r32 patternScale = outputScale * 2;

    ImGui::SetNextWindowPos( pDisplay, ImGuiCond_Always );
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

    const u32 patternsCount = globalState->input.patterns.count;
    const u32 patternsCountSqrt = (u32)Round( Sqrt( (r32)patternsCount ) );
    const v3u waveDim = GetWaveDim( spec );
    const u32 waveLength = waveDim.x * waveDim.y;

    ImGui::BeginChild( "child_wfc_right", ImVec2( 0, 0 ), true, 0 );
    switch( displayState->currentPage )
    {
        case TestPage::Output:
        {
            ImGui::BeginChild( "child_output_meta", ImVec2( 350, 0 ) );

            // TODO Show relevant 'totals' info and have a 'selected' chunk to which following info belongs
            const ChunkInfo& chunkInfo = globalState->outputChunks[0];

            if( chunkInfo.buildJob && chunkInfo.buildJob->state )
            {
                State* state = chunkInfo.buildJob->state;

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
            }

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild( "child_output_image", ImVec2( 1100, 0 ) );
            v2u imageSize = { displayState->outputTexture.width, displayState->outputTexture.height };

            bool inProgress = JobsInProgress( globalState );
            if( !displayState->allDone )
            {
                // Do one more pass after all progress stopped
                // TODO Very Broken Synchronization, but I don't really care right now!
                displayState->allDone = !inProgress;

                if( !displayState->outputImageBuffer )
                    displayState->outputImageBuffer = PUSH_ARRAY( wfcDisplayArena, u32, imageSize.x * imageSize.y );

                CreateOutputTexture( spec, globalState, displayState->outputImageBuffer, &displayState->outputTexture );
            }

            ImGui::Image( displayState->outputTexture.handle, imageSize * outputScale );

            if( ImGui::IsItemClicked( 2 ) )
                selectedIndex = displayState->currentSpecIndex;

            ImGui::EndChild();
            ImGui::SameLine();

            ImGui::BeginChild( "child_debug", ImVec2( 0, 0 ) );
            if( debugState )
            {
                DrawPerformanceCounters( debugState, tmpMemory );
            }
            ImGui::EndChild();

        } break;

        case TestPage::Patterns:
        {
            if( !displayState->patternTextures )
                displayState->patternTextures = Array<Texture>( wfcDisplayArena, patternsCount, patternsCount );

            bool firstRow = true;
            ImGui::Columns( patternsCountSqrt, nullptr, false );
            for( u32 i = 0; i < patternsCount; ++i )
            {
                const Pattern& p = globalState->input.patterns[i];
                Texture& t = displayState->patternTextures[i];

                if( !t.handle )
                    t = CreatePatternTexture( p, globalState->input.palette, wfcDisplayArena );

                ImGui::Text( "%u.", i );
                ImGui::SameLine();
                ImGui::Image( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) );
                ImGui::SameLine();
                ImGui::Text( "x %u", globalState->input.frequencies[i] );

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
                const Pattern& p = globalState->input.patterns[i];
                Texture& t = displayState->patternTextures[i];

                if( !t.handle )
                    t = CreatePatternTexture( p, globalState->input.palette, wfcDisplayArena );

                if( ImGui::ImageButton( t.handle, ImVec2( (r32)t.width * patternScale, (r32)t.height * patternScale ) ) )
                    displayState->currentIndexEntry = i;
                ImGui::Spacing();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild( "child_index_offsets", ImVec2( 0, 0 ), false, 0 );
            if( globalState->processedChunkCount )
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
                            d = (int)Adjacency::Left;
                        else if( i == 1 && j == 2 )
                            d = (int)Adjacency::Bottom;
                        else if( i == 2 && j == 1 )
                            d = (int)Adjacency::Right;
                        else if( i == 1 && j == 0 )
                            d = (int)Adjacency::Top;

                        u32 row = 0;
                        if( d >= 0 )
                        {
                            const Array<bool>& matches
                                = globalState->input.patternsIndex[d].entries[displayState->currentIndexEntry].matches;

                            for( u32 m = 0; m < patternsCount; ++m )
                            {
                                if( !matches[m] )
                                    continue;

                                const Pattern& p = globalState->input.patterns[m];
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
