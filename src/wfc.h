#ifndef __WFC_H__
#define __WFC_H__ 



// TODO Review all TODOs!!
// x Add counters so we can start getting a sense of what's what
// x Add a maximum snapshot stack size and tweak the snapshot sampling curve so it's easier to get to early snapshots
// x Make the snapshot stack non-circular and make sure we're always backtracking from the correct snapshot
// x Pack wave data in bits similar to the adjacency counters to keep lowering memory consumption (check sizes!)
// x Do tiled multithreading
// - 3D ffs!!
//

namespace WFC
{
    // Determines the size of the ring cache for cells that are "blacklisted" temporarily during the backtracking process
    // (0 = disabled)
    const u32 BacktrackedCellsCacheCount = 0;
    // Cap for the size of the snapshot stack, since beyond a certain point, more continuous snapshots means backtracking is less effective
    // Probably will need to be calibrated against the final sets that we use
    const u32 MaxBacktrackingDepth = 64;
    // Bigger means more U shaped, which makes it more likely to revisit very early steps in the solving process when backtracking
    // This will also need some tweaking with for final examples
    const r32 ObservationCountParabolaDegree = 8;


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
        { Left,     { -1,  0,  0 }, Right,  (u64)Left * BitsPerAxis,     (u64(MaxAdjacencyCount - 1)) << (Left * BitsPerAxis) },
        { Bottom,   {  0,  1,  0 }, Top,    (u64)Bottom * BitsPerAxis,   (u64(MaxAdjacencyCount - 1)) << (Bottom * BitsPerAxis) },
        { Right,    {  1,  0,  0 }, Left,   (u64)Right * BitsPerAxis,    (u64(MaxAdjacencyCount - 1)) << (Right * BitsPerAxis) },
        { Top,      {  0, -1,  0 }, Bottom, (u64)Top * BitsPerAxis,      (u64(MaxAdjacencyCount - 1)) << (Top * BitsPerAxis) },
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

        v2u outputChunkDim;         // In voxels
        v2u outputChunkCount;
        bool periodic;
    };
    inline Spec DefaultSpec()
    {
        Spec result =
        {
            "default",
            {},
            2,
            { 64, 64 },
            { 4, 4 },
            false,
        };
        return result;
    }

    enum Result : u32
    {
        NotStarted = 0,
        InProgress,
        Contradiction,
        Cancelled,
        Failed,
        Done,
    };


    struct Input
    {
        u8* inputSamples;
        u32 palette[256];
        u32 paletteEntryCount;

        HashTable<Pattern, u32, PatternHash> patternsHash;
        // TODO Cache patternCount and substitute everywhere
        Array<Pattern> patterns;
        Array<u32> frequencies;                         // weights
        Array<r64> weights;                             // weightLogWeights
        u32 sumFrequencies;
        r64 sumWeights;
        r64 initialEntropy;

        // TODO Layout this differently (and rename it)
        IndexCell patternsIndex[Adjacency::Count];      // propagator
        u32 waveLength;
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
    };

    struct State
    {
        MemoryArena* arena;

        Array<BannedTuple> propagationStack;
        Array<r32> distributionTemp;

        RingBuffer<u32> backtrackedCellIndices;
        Array<Snapshot> snapshotStack;
        Snapshot* currentSnapshot;

        u32 observationCount;
        u32 contradictionCount;
        Result currentResult;
    };

    struct JobMemory
    {
        MemoryArena arena;
        volatile bool inUse;
    };

    struct ChunkInfo;

    struct Job
    {
        // In
        const Spec* spec;
        const Input* input;
        ChunkInfo* outputChunk;

        State* state;           // Only for visualization
        JobMemory* memory;

        volatile bool inUse;
        volatile bool cancellationRequested;
    };

    struct ChunkInfo
    {
        Job* buildJob;          // Only for visualization
        Array2<u8> outputSamples;
        volatile Result result;
        bool done;
    };

    struct GlobalState
    {
        Spec spec;
        Array<Job> jobs;
        // NOTE We keep these separate from the jobs because we may want to decouple them in the future
        Array<JobMemory> jobMemoryChunks;
        Array2<ChunkInfo> outputChunks;
        MemoryArena* globalWFCArena;

        Input input;
        u32 processedChunkCount;
        bool cancellationRequested;
        bool done;
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
        u32 currentIndexEntry;

        Array<Texture> patternTextures;
        u32* outputImageBuffer;
        Texture outputTexture;

        bool allDone;
    };

} // namespace WFC

#endif /* __WFC_H__ */
