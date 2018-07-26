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

struct DebugCycleCounter
{
    u32 lineNumber;
    const char* filename;
    const char* function;

    u64 frameCycles;
    u64 totalCycles;
    u32 frameHitCount;
    u32 totalHitCount;
};

struct DebugGameStats
{
    // NOTE Since we use __COUNTER__ for indexing, we'd need a separate array for platform counters
    DebugCycleCounter *gameCounters;
    u32 gameCountersCount;

    u32 totalDrawCalls;
    u32 totalVertexCount;
    u32 totalPrimitiveCount;
};
extern DebugGameStats* DEBUGglobalStats;

inline void
ResetFrameCounters( DebugGameStats* stats )
{
    for( u32 i = 0; i < stats->gameCountersCount; ++i )
    {
        DebugCycleCounter& c = stats->gameCounters[i];
        c.frameCycles = 0;
        c.frameHitCount = 0;
    }
}

struct DebugTimedBlock
{
    u32 index;
    u32 lineNumber;
    const char* filename;
    const char* function;
    u64 startCycleCount;

    DebugTimedBlock( u32 index_, u32 lineNumber_, const char* filename_, const char* function_ )
    {
        index = index_;
        lineNumber = lineNumber_;
        filename = filename_;
        function = function_;

        startCycleCount = __rdtsc();
    }

    ~DebugTimedBlock()
    {
        u64 cycleCount = __rdtsc() - startCycleCount;

        DebugCycleCounter& c = DEBUGglobalStats->gameCounters[index];
        c.lineNumber = lineNumber;
        c.filename = filename;
        c.function = function;

        c.frameCycles += cycleCount;
        c.totalCycles += cycleCount;
        ++c.frameHitCount;
        ++c.totalHitCount;
    }
};

#define TIMED_BLOCK DebugTimedBlock __timedBlock( __COUNTER__, __LINE__, __FILE__, __FUNCTION__ );

#else

#define TIMED_BLOCK

#endif // DEBUG

#endif /* __DEBUGSTATS_H__ */
