/*
The MIT License

Copyright (c) 2017 Oscar Peñas Pariente <oscarpp80@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __DEBUGSTATS_H__
#define __DEBUGSTATS_H__ 

#if DEBUG

enum class DebugCycleCounterId : u32
{
    GameUpdateAndRender,
    LoadEntitiesInCluster,
    GenerateEntities,
    MarchAreaFast,
    MarchCube,
    SampleFunc,
};

struct DebugCycleCounter
{
    const char* name;

    u64 frameCycles;
    u64 totalCycles;
    u32 frameHitCount;
    u32 totalHitCount;
};

struct DebugGameStats
{
    DebugCycleCounter counters[4096];

    u32 totalDrawCalls;
    u32 totalVertexCount;
    u32 totalPrimitiveCount;
};

extern DebugGameStats* DEBUGglobalStats;

inline void
ResetFrameCounters( DebugGameStats* stats )
{
    for( u32 i = 0; i < ARRAYCOUNT(stats->counters); ++i )
    {
        DebugCycleCounter& c = stats->counters[i];
        c.frameCycles = 0;
        c.frameHitCount = 0;
    }
}

struct DebugTimedBlock
{
    DebugCycleCounterId id;
    const char* name;
    u64 startCycleCount;

    DebugTimedBlock( DebugCycleCounterId id_, const char* name_ )
    {
        id = id_;
        name = name_;
        startCycleCount = __rdtsc();
    }

    ~DebugTimedBlock()
    {
        u64 cycleCount = __rdtsc() - startCycleCount;
        DebugCycleCounter& c = DEBUGglobalStats->counters[(u32)id];
        c.name = name;
        c.frameCycles += cycleCount;
        c.totalCycles += cycleCount;
        ++c.frameHitCount;
        ++c.totalHitCount;
    }
};

#define TIMED_BLOCK(id) DebugTimedBlock block##id( DebugCycleCounterId::id, #id );

#endif // DEBUG

#endif /* __DEBUGSTATS_H__ */
