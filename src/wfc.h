#ifndef __WFC_H__
#define __WFC_H__ 

namespace WFC
{


struct Pattern
{
    u8* data;
    u32 stride;

    Texture texture;        // Just for debugging
};

// TODO Add later as an optimization
// TODO Test also adding paddings for cache niceness
struct PackedPattern
{
    u32 hash; //?
    u32 frequency;
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

enum class TestPage
{
    Output,
    Patterns,
    Index,
};

enum Result
{
    NotStarted = 0,
    InProgress,
    Done,
    Contradiction,
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
    Texture source;
    u32 N;

    v2i outputDim;
    bool periodic;

    v2 displayDim;
    bool displayMode;       // true - display intermediate results every frame
};

struct State
{
    u8* input;
    u32 palette[256];
    u32 paletteEntries;

    HashTable<Pattern, u32, PatternHash> patternsHash;
    Array<Pattern> patterns;

    // TODO Layout this differently (and rename it)
    IndexCell patternsIndex[Adjacency::Count];      // propagator
    Array<BannedTuple> propagationStack;

    Array2<bool> wave;      // TODO Consider packing this into u64 flags even if that limits the maximum patterns

    Array2<AdjacencyCounters> adjacencyCounters;    // compatible
    // TODO This might be redundant? (just count how many patterns in a wave cell are still true)
    Array<u32> compatiblesCount;                    // sumsOfOnes

    Array<r64> weights;                             // weightLogWeights
    Array<r64> sumFrequencies;                      // sumsOfWeights
    Array<r64> sumWeights;                          // sumsOfWeightLogWeights
    Array<r64> entropies;

    Array<u32> observations;

    Result currentResult;

    // UI stuff
    TestPage currentPage;
    u32 currentIndexEntry;
    u32* outputImage;
    void* outputTextureHandle;
    Texture outputTexture;
    u32 remaining;
    u32 lastRemaining;
    DEBUGFileInfoList currentFileList;
    char currentRootPath[PLATFORM_PATH_MAX];
};


} // namespace WFC

#endif /* __WFC_H__ */
