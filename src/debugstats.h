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
    const char* filename;
    const char* function;
    u32 lineNumber;

    volatile u32 totalHitCount;
    volatile u64 totalCycleCount;
    volatile u64 frameHitCount24CycleCount40;
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
UnpackAndResetFrameCounter( DebugCycleCounter& c, u64* frameCycleCount, u32* frameHitCount )
{
    u64 result = AtomicExchangeU64( &c.frameHitCount24CycleCount40, 0 );
    
    *frameHitCount = (u32)(result >> 40);
    *frameCycleCount = (result & 0xFFFFFFFFFF);

    AtomicAddU32( &c.totalHitCount, *frameHitCount );
    AtomicAddU64( &c.totalCycleCount, *frameCycleCount );
}

struct DebugTimedBlock
{
    DebugCycleCounter& counter;
    u64 startCycleCount;

    DebugTimedBlock( u32 index_, u32 lineNumber_, const char* filename_, const char* function_ )
        : counter( DEBUGglobalStats->gameCounters[index_] )
    {
        counter.lineNumber = lineNumber_;
        counter.filename = filename_;
        counter.function = function_;

        startCycleCount = __rdtsc();
    }

    ~DebugTimedBlock()
    {
        u64 cycleCount = __rdtsc() - startCycleCount;

        u64 frameDelta = ((u64)1 << 40) | (cycleCount & 0xFFFFFFFFFF);
        AtomicAddU64( &counter.frameHitCount24CycleCount40, frameDelta );
    }
};

#define TIMED_BLOCK DebugTimedBlock __timedBlock( __COUNTER__, __LINE__, __FILE__, __FUNCTION__ );

#else

#define TIMED_BLOCK

#endif // DEBUG

#endif /* __DEBUGSTATS_H__ */
