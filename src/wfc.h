#ifndef __WFC_H__
#define __WFC_H__ 

struct WFCPattern
{
    u8* data;
    u32 width;

    Texture texture;        // Just for debugging
};

// TODO Add later as an optimization
// TODO Test also adding paddings for cache niceness
struct WFCPackedPattern
{
    u32 hash; //?
    u32 frequency;
    u8 data[];
};

inline u32 PatternHash( const WFCPattern& key, u32 tableSize );

struct WFCIndexEntry
{
    // One entry per pattern
    Array<bool> matches;
};

// This might change
struct WFCIndexCell
{
    // One entry per pattern
    Array<WFCIndexEntry> entries;
};

enum class WFCTestPage
{
    Patterns,
    Index,
};

struct WFCState
{
    u8* input;
    u32 inputWidth;
    u32 inputHeight;
    u32 inputChannels;

    u32 palette[256];
    u32 paletteEntries;

    HashTable<WFCPattern, u32, PatternHash> patternsHash;
    Array<WFCPattern> patterns;

    // 4 directions for 2D
    // This might change
    WFCIndexCell patternsIndex[4];

    bool done;

    // UI stuff
    WFCTestPage currentPage;
    u32 currentIndexEntry;
};


#endif /* __WFC_H__ */
