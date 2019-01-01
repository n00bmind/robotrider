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


// Minimal counter tied to a specific code location
struct DebugCycleCounter
{
    const char* filename;
    const char* function;
    u32 lineNumber;

    volatile u64 frameHitCount24CycleCount40;
};

struct DebugCounterSnapshot
{
    u32 hitCount;
    u64 cycleCount;
};

struct DebugCounterLog
{
    const char* filename;
    const char* function;
    u32 lineNumber;

    u32 totalHitCount;
    u64 totalCycleCount;

    DebugCounterSnapshot snapshots[300];
};

inline void
UnpackAndResetFrameCounter( DebugCycleCounter* c, DebugCounterLog* dest, u32 snapshotIndex )
{
    u64 result = AtomicExchange( &c->frameHitCount24CycleCount40, 0 );

    u32 frameHitCount = (u32)(result >> 40);
    u64 frameCycleCount = (result & 0xFFFFFFFFFF);

    if( !dest->filename )
    {
        dest->filename = c->filename;
        dest->function = c->function;
        dest->lineNumber = c->lineNumber;
    }
    dest->snapshots[snapshotIndex].hitCount = frameHitCount;
    dest->snapshots[snapshotIndex].cycleCount = frameCycleCount;
    AtomicAdd( &dest->totalHitCount, frameHitCount );
    AtomicAdd( &dest->totalCycleCount, frameCycleCount );
}


struct DebugState
{
    // NOTE Since we use __COUNTER__ for indexing, we'd need a separate array for platform counters
    DebugCounterLog counterLogs[1024];
    u32 counterLogsCount;
    u32 counterSnapshotIndex;

    u32 totalDrawCalls;
    u32 totalVertexCount;
    u32 totalGeneratedVerticesCount;
    u32 totalPrimitiveCount;
    u32 totalEntities;
};


extern DebugCycleCounter DEBUGglobalCounters[];

struct DebugTimedBlock
{
    DebugCycleCounter& counter;
    u64 startCycleCount;

    DebugTimedBlock( u32 index_, u32 lineNumber_, const char* filename_, const char* function_ )
        : counter( DEBUGglobalCounters[index_] )
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
        AtomicAdd( &counter.frameHitCount24CycleCount40, frameDelta );
    }
};

#if RELEASE
#define TIMED_BLOCK
#else
#define TIMED_BLOCK DebugTimedBlock __timedBlock( __COUNTER__, __LINE__, __FILE__, __FUNCTION__ );
#endif


#endif /* __DEBUGSTATS_H__ */
