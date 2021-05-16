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

#if NON_UNITY_BUILD
#include "common.h"
#include "intrinsics.h"
#endif

// Minimal counter tied to a specific code location
struct DebugCycleCounter
{
    volatile u64 frameHitCount24CycleCount40;

    const char* name;
    const char* filename;
    u32 lineNumber;
    bool computeTotals;
};

struct DebugCounterSnapshot
{
    u64 frameCycles;
    f32 frameMillis;
    u32 frameHits;
};

struct DebugCounterLog
{
    DebugCounterSnapshot snapshots[300];

    const char* name;
    const char* filename;
    u32 lineNumber;

    volatile u32 totalHits;
    volatile u64 totalCycles;
    volatile f32 totalMillis;

    bool computeTotals;
};

inline void
LogFrameCounter( DebugCycleCounter* c, DebugCounterLog* log, int snapshotIndex, f64 cyclesPerMillisecond )
{
    u64 result = AtomicExchange( &c->frameHitCount24CycleCount40, 0 );
    u32 frameHitCount = (u32)(result >> 40);
    u64 frameCycleCount = (result & 0xFFFFFFFFFF);

    DebugCounterSnapshot& snapshot = log->snapshots[snapshotIndex];
    snapshot.frameHits = frameHitCount;
    snapshot.frameCycles = frameCycleCount;
    snapshot.frameMillis = (f32)(frameCycleCount / cyclesPerMillisecond);

    if( !log->name )
    {
        log->name = c->name;
        log->filename = c->filename;
        log->lineNumber = c->lineNumber;
        log->computeTotals = c->computeTotals;
    }

    if( log->computeTotals )
    {
        AtomicAdd( &log->totalHits, frameHitCount );
        AtomicAdd( &log->totalCycles, frameCycleCount );
    }
}


struct DebugState
{
    // NOTE Since we use __COUNTER__ for indexing, we'd need a separate array for platform counters
    // FIXME Use a constexpr counter instead
    DebugCounterLog counterLogs[1024];
    i32 counterLogsCount;
    i32 counterSnapshotIndex;
    f64 avgCyclesPerMillisecond;

    u32 totalDrawCalls;
    u32 totalVertexCount;
    u32 totalPrimitiveCount;
    u32 totalInstanceCount;
    u32 totalGeneratedVerticesCount;
    i32 totalEntities;
    u32 totalMeshCount;
};


extern DebugCycleCounter DEBUGglobalCounters[];

struct DebugTimedBlock
{
    DebugCycleCounter& counter;
    u64 startCycleCount;

    DebugTimedBlock( u32 index, char const* name, char const* filename, u32 lineNumber, bool computeTotals = false )
        : counter( DEBUGglobalCounters[index] )
    {
        counter.name = name;
        counter.filename = filename;
        counter.lineNumber = lineNumber;
        counter.computeTotals = computeTotals;

        startCycleCount = Rdtsc();
    }

    ~DebugTimedBlock()
    {
        u64 totalCycles = Rdtsc() - startCycleCount;

        u64 frameDelta = ((u64)1 << 40) | (totalCycles & 0xFFFFFFFFFF);
        AtomicAdd( &counter.frameHitCount24CycleCount40, frameDelta );
    }
};

#if RELEASE
#define TIMED_FUNC
#define TIMED_SCOPE(...)
#define TIMED_FUNC_WITH_TOTALS
#define TIMED_SCOPE_WITH_TOTALS(...)
#else
#define TIMED_FUNC DebugTimedBlock __timedBlock( __COUNTER__, __FUNCTION__, __FILE__, __LINE__ );
#define TIMED_SCOPE(name) DebugTimedBlock __timedBlock( __COUNTER__, name, __FILE__, __LINE__ );
#define TIMED_FUNC_WITH_TOTALS DebugTimedBlock __timedBlock( __COUNTER__, __FUNCTION__, __FILE__, __LINE__, true );
#define TIMED_SCOPE_WITH_TOTALS(name) DebugTimedBlock __timedBlock( __COUNTER__, name, __FILE__, __LINE__, true );
#endif


#endif /* __DEBUGSTATS_H__ */
