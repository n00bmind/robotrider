#ifndef __WFC_H__
#define __WFC_H__ 

#if NON_UNITY_BUILD
#include "data_types.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// So... We finally managed to find an inter-chunk propagation pattern that works! (sort of)
// It seems the best method is to leave a "safe margin" of a certain width on each side of each wave, and then copy just the inner area
// (the area that truly belongs to each chunk) when propagating to a neighbour.
// (see Sketchup diagram in docs/wfc_merging_tiles.skp)
//  
// However, some cells are _still_ causing contradictions, which is incredibly weird since the very same patterns in the corresponding
// cells of the source chunk were obviously playing along nicely (otherwise it wouldn't have completed).
// I've managed to ignore the errors by just forcing the collapsed wave cell to the pattern we want and everything seems to work out
// in the end, so who cares... right?
//
// Also, the way we're doing multithreading can be made faster by extending the neighbouring chunks we examine when deciding whether to
// start a new one to also include the diagonal chunks (see UpdateWFCAsync).
// Finally, there's surely a few quirks to iron out like the issue with the pattern frequencies below.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// x Add counters so we can start getting a sense of what's what
// x Add a maximum snapshot stack size and tweak the snapshot sampling curve so it's easier to get to early snapshots
// x Make the snapshot stack non-circular and make sure we're always backtracking from the correct snapshot
// x Pack wave data in bits similar to the adjacency counters to keep lowering memory consumption (check sizes!)
// x Do tiled multithreading
// - FIXME Pattern frequencies are all wrong (see displayed data in DrawTest)
// - 3D (someday)
//


// TODO Inline and collapse functions where possible


struct DebugState;

namespace WFC
{
    // Determines the size of the ring cache for cells that are "blacklisted" temporarily during the backtracking process
    // (0 = disabled)
    const int BacktrackedCellsCacheCount = 0;
    // Cap for the size of the snapshot stack, since beyond a certain point, more continuous snapshots means backtracking is less effective
    // Probably will need to be calibrated against the final sets that we use
    const int MaxBacktrackingDepth = 64;
    // Bigger means more U shaped, which makes it more likely to revisit very early steps in the solving process when backtracking
    // This will also need some tweaking with for final examples
    const f32 ObservationCountParabolaDegree = 8;


    struct Pattern
    {
        u8* data;
        i32 stride;
    };

    // TODO Add later as an optimization
    // TODO Test also adding paddings for cache niceness
    struct PackedPattern
    {
        u32 hash; //?
        i32 frequency;
        i32 stride;
        u8 data[];
    };

    inline u32 PatternHash( const Pattern& key, i32 tableSize );

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

    // (still 4 bits free in each u64)
    const int BitsPerAxis = 10;
    const int MaxAdjacencyCount = 1 << BitsPerAxis;

    struct AdjacencyMeta
    {
        Adjacency dir;
        v3i cellDelta;
        i32 oppositeIndex;
        u64 counterExp;
        u64 counterMask;
    };
    const AdjacencyMeta AdjacencyMeta[] =
    {
        { Left,     { -1,  0,  0 }, Right,  (u64)Left * BitsPerAxis,     (u64(MaxAdjacencyCount - 1)) << (Left * BitsPerAxis) },
        { Bottom,   {  0,  1,  0 }, Top,    (u64)Bottom * BitsPerAxis,   (u64(MaxAdjacencyCount - 1)) << (Bottom * BitsPerAxis) },
        { Right,    {  1,  0,  0 }, Left,   (u64)Right * BitsPerAxis,    (u64(MaxAdjacencyCount - 1)) << (Right * BitsPerAxis) },
        { Top,      {  0, -1,  0 }, Bottom, (u64)Top * BitsPerAxis,      (u64(MaxAdjacencyCount - 1)) << (Top * BitsPerAxis) },
    };

    struct BannedTuple
    {
        i32 cellIndex;
        i32 patternIndex;
    };

    struct Spec
    {
        const char* name;

        Texture source;
        i32 N;

        v3i outputChunkDim;         // In voxels
        v3i outputChunkCount;
        i32 safeMarginWidth;        // Extra wave cells to add on top of the actual chunk dim
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
            5,
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
        i32 paletteEntryCount;

        HashTable<Pattern, i32, PatternHash> patternsHash;
        Array<Pattern> patterns;
        Array<i32> frequencies;                         // weights
        Array<f64> weights;                             // weightLogWeights
        i32 sumFrequencies;
        f64 sumWeights;
        f64 initialEntropy;

        // TODO Layout this differently (and rename it)
        IndexCell patternsIndex[Adjacency::Count];      // propagator
    };

    struct Snapshot
    {
        Array2<u64> wave;
        // How many patterns are still compatible in each adjacent direction (for every pattern at every cell)
        Array2<u64> adjacencyCounters;                  // compatible
        // @Memory This could be extracted from the wave (although slower)
        // (http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetKernighan)
        Array<i32> compatiblesCount;                    // sumsOfOnes

        Array<f64> sumFrequencies;                      // sumsOfWeights
        Array<f64> sumWeights;                          // sumsOfWeightLogWeights
        Array<f64> entropies;

        Array<f32> distribution;
        // Last random choice that was made in this snapshot, so we can undo it when rewinding
        i32 lastObservedDistributionIndex;
        // Last observed cell during this snapshot, so we can discard it when backtracking
        i32 lastObservedCellIndex;

        i32 lastObservationCount;
    };

    struct State
    {
        MemoryArena* arena;

        Array<BannedTuple> propagationStack;
        Array<f32> distributionTemp;

        RingBuffer<i32> backtrackedCellIndices;
        Array<Snapshot> snapshotStack;
        Snapshot* currentSnapshot;

        i32 observationCount;
        i32 contradictionCount;
        volatile Result currentResult;
    };

    struct Job;

    struct ChunkInfo
    {
        Job* buildJob;          // Only for visualization
        Array<i32> collapsedWave;
        Array2<u8> outputSamples;

        v2i pChunk;
        volatile Result result;
        bool canProceed;
        bool done;
    };

    struct ChunkInitInfo
    {
        const Snapshot* snapshot0;
        sz snapshot0Size;
        const Array<i32>* adjacentChunks[Adjacency::Count];
    };

    struct JobMemory
    {
        MemoryArena arena;
        volatile bool inUse;
    };

    struct Job
    {
        // In
        const Spec* spec;
        const Input* input;
        ChunkInfo* outputChunk;
        ChunkInitInfo initInfo;

        State* state;           // Only for visualization
        JobMemory* memory;

        volatile bool inUse;
        volatile bool cancellationRequested;
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
        Snapshot snapshot0;
        sz snapshot0Size;

        i32 processedChunkCount;
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
        i32 currentSpecIndex;
        i32 currentIndexEntry;

        Array<Texture> patternTextures;
        u32* outputImageBuffer;
        Texture outputTexture;

        bool allDone;
    };



    GlobalState* StartWFCAsync( const Spec& spec, const v2i& pStartChunk, MemoryArena* wfcArena );
    void UpdateWFCAsync( GlobalState* globalState );
    int DrawTest( const Array<Spec>& specs, const GlobalState* globalState, DisplayState* displayState,
                  const v2& pDisplay, const v2& displayDim, const DebugState* debugState, MemoryArena* wfcDisplayArena,
                  const TemporaryMemory& tmpMemory );


} // namespace WFC

#endif /* __WFC_H__ */
