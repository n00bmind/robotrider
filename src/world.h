#ifndef __WORLD_H__
#define __WORLD_H__ 

// Entry points are the start of a new generation 'path'
struct GenEntryPoint
{
    v3 pStart;
    v3 vDirection;
    GenEntryPoint *next;
};

struct World
{
    // Review how Muratori implements this, specifically how to allocate nodes that will at some point be destroyed
    GenEntryPoint *pendingEntryPointsQHead;
    GenEntryPoint *pendingEntryPointsQTail;

};

#endif /* __WORLD_H__ */
