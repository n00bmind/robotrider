#ifndef __WFC_H__
#define __WFC_H__ 

namespace WFC
{


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
    Left,
    Bottom,
    Right,
    Top,
    // Front,
    // Back,

    Count
};

// How many patterns are still compatible in each direction
// TODO Consider packing all counts into a u64 even if that limits the maximum patterns
struct AdjacencyCounters
{
    u32 dir[Adjacency::Count];
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
inline Spec
DefaultSpec()
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
    Cancelled,
    Done,
    Contradiction,
};

struct Snapshot
{
    Array2<bool> wave;      // TODO Consider packing this into u64 flags even if that limits the maximum patterns

    Array2<AdjacencyCounters> adjacencyCounters;    // compatible
    // TODO This might be redundant? (just count how many patterns in a wave cell are still true)
    Array<u32> compatiblesCount;                    // sumsOfOnes

    Array<r64> sumFrequencies;                      // sumsOfWeights
    Array<r64> sumWeights;                          // sumsOfWeightLogWeights
    Array<r64> entropies;

    Array<r32> distribution;
    // TODO Put the last random choice that was made here,
    // so we know which option to discard next time we come back to this snapshot
    // When a contradiction is found, go back to the last available snapshot, put a 0 in the corresponding distribution entry, and RandomSelect again
    u32 selectedDistributionIndex;

    sz highWaterMark;
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

    RingStack<Snapshot> snapshotStack;
    Snapshot* currentSnapshot;

    Result currentResult;
    u32 remainingObservations;
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
    u32 currentIndexEntry;

    Array<Texture> patternTextures;
    u32* outputImageBuffer;
    Texture outputTexture;

    Result lastDisplayedResult;
    u32 lastRemainingObservations;
};

struct WFCJob
{
    Spec spec;
    State* state;
    MemoryArena* arena;
};

} // namespace WFC

#endif /* __WFC_H__ */
