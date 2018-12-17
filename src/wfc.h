#ifndef __WFC_H__
#define __WFC_H__ 



// TODO Review all TODOs!!
// x Add counters so we can start getting a sense of what's what
// x Add a maximum snapshot stack size and tweak the snapshot sampling curve so it's easier to get to early snapshots
// - Make the snapshot stack non-circular and make sure we're always backtracking from the correct snapshot
// - Pack wave data in bits similar to the adjacency counters to keep lowering memory consumption (check sizes!)
// - Do tiled multithreading
// - 3D ffs!!
//

namespace WFC
{
    const u32 BacktrackedCellsCacheCount = 0;


    struct Pattern
    {
        u8* data;
        u32 stride;
    };

    // TODO Add later as an optimization
    // TODO Test also adding paddings for cache niceness
    struct PackedPattern
    {
        u32 hash; //?
        u32 frequency;
        u32 stride;
        u8 data[];
    };

    inline u32 PatternHash( const Pattern& key, u32 tableSize );

    struct IndexEntry
    {
        // One entry per pattern
        Array<bool> matches;
    };

    // This might change
    struct IndexCell
    {
        // One entry per pattern
        Array<IndexEntry> entries;
    };

    enum Adjacency
    {
        Left = 0,
        Bottom,
        Right,
        Top,
        // Front,
        // Back,

        Count
    };

    const u32 BitsPerAxis = 10;
    const u32 MaxAdjacencyCount = 1 << BitsPerAxis;

    struct AdjacencyMeta
    {
        Adjacency dir;
        v3i cellDelta;
        u32 oppositeIndex;
        u64 counterExp;
        u64 counterMask;
    };
    const AdjacencyMeta adjacencyMeta[] =
    {
        { Left,     { -1,  0,  0 }, Right,  (u64)Left * BitsPerAxis,     u64(MaxAdjacencyCount - 1) << (Left * BitsPerAxis) },
        { Bottom,   {  0,  1,  0 }, Top,    (u64)Bottom * BitsPerAxis,   u64(MaxAdjacencyCount - 1) << (Bottom * BitsPerAxis) },
        { Right,    {  1,  0,  0 }, Left,   (u64)Right * BitsPerAxis,    u64(MaxAdjacencyCount - 1) << (Right * BitsPerAxis) },
        { Top,      {  0, -1,  0 }, Bottom, (u64)Top * BitsPerAxis,      u64(MaxAdjacencyCount - 1) << (Top * BitsPerAxis) },
    };

    struct BannedTuple
    {
        u32 cellIndex;
        u32 patternIndex;
    };

    struct Spec
    {
        const char* name;

        Texture source;
        u32 N;

        v2i outputDim;
        bool periodic;
    };
    inline Spec DefaultSpec()
    {
        Spec result =
        {
            "default",
            {},
            2,
            { 256, 256 },
            true,
        };
        return result;
    }

    enum Result
    {
        NotStarted = 0,
        InProgress,
        Contradiction,
        Cancelled,
        Failed,
        Done,
    };

    struct Snapshot
    {
        Array2<u64> wave;

        // How many patterns are still compatible (with every pattern at every cell) in each direction
        // (still 4 bits free here)
        Array2<u64> adjacencyCounters;                  // compatible
        // NOTE This could be extracted from the wave (although slower)
        Array<u32> compatiblesCount;                    // sumsOfOnes

        Array<r64> sumFrequencies;                      // sumsOfWeights
        Array<r64> sumWeights;                          // sumsOfWeightLogWeights
        Array<r64> entropies;

        Array<r32> distribution;
        // Last random choice that was made in this snapshot, so we can undo it when rewinding
        u32 lastObservedDistributionIndex;
        // Last observed cell during this snapshot, so we can discard it when backtracking
        u32 lastObservedCellIndex;

        u32 lastObservationCount;
        //u32 lastObservationWidth;
    };


    struct State
    {
        MemoryArena* arena;

        u8* input;
        u32 palette[256];
        u32 paletteEntries;

        // TODO Consolidate this stuff. We're duplicating data here!
        HashTable<Pattern, u32, PatternHash> patternsHash;
        Array<Pattern> patterns;
        Array<u32> frequencies;                         // weights
        Array<r64> weights;                             // weightLogWeights

        // TODO Layout this differently (and rename it)
        IndexCell patternsIndex[Adjacency::Count];      // propagator
        Array<BannedTuple> propagationStack;
        Array<r32> distributionTemp;

        RingBuffer<u32> backtrackedCellIndices;
        Array<Snapshot> snapshotStack;
        Snapshot* currentSnapshot;

        u32 observationCount;
        u32 lastSnapshotObservationCount;
        u32 contradictionCount;
        Result currentResult;

        mutable bool cancellationRequested;
    };

    enum class TestPage
    {
        Output,
        Patterns,
        Index,
    };

    struct DisplayState
    {
        TestPage currentPage;
        u32 currentSpecIndex;
        u32 requestedSpecIndex;
        u32 currentIndexEntry;

        Array<Texture> patternTextures;
        u32* outputImageBuffer;
        Texture outputTexture;

        Result lastDisplayedResult;
        u32 lastTotalObservations ;
    };

    struct WFCJob
    {
        Spec spec;
        State* state;
        MemoryArena* arena;
    };

} // namespace WFC

#endif /* __WFC_H__ */
