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

#ifndef __MEMORY_H__
#define __MEMORY_H__ 

#if NON_UNITY_BUILD
#include <string.h>
#include "common.h"
#include "intrinsics.h"
#endif


// TODO ASAP Add a tag per allocation (even the same arena can contain different stuff)
#define PUSH_STRUCT(arena, type, ...) (type *)_PushSize( arena, sizeof(type), ## __VA_ARGS__ )
#define PUSH_ARRAY(arena, type, count, ...) (type *)_PushSize( arena, (count)*sizeof(type), ## __VA_ARGS__ )
#define PUSH_SIZE(arena, size, ...) _PushSize( arena, size, ## __VA_ARGS__ )


///// STATIC MEMORY ARENA
// Linear memory arena of a fixed initial size
// Can be partitioned into sub arenas and supports "temporary blocks" (which can be nested)

struct MemoryArena
{
    u8 *base;
    sz size;
    sz used;

    u32 tempCount;
};

enum MemoryFlags
{
    MemoryFlags_None = 0,
    MemoryFlags_ClearToZero = 0x1,                  // Zeroed upon allocation
    MemoryFlags_TemporaryMemory = 0x2,              // No guaranteed persistence beyond allocation scope
};

struct MemoryParams
{
    u32 flags;
    u32 alignment;
};

inline void
InitArena( MemoryArena *arena, u8 *base, sz size )
{
    arena->base = base;
    arena->size = size;
    arena->used = 0;
    arena->tempCount = 0;
}

inline void
ClearArena( MemoryArena* arena, bool clearToZero = true )
{
    arena->used = 0;
    // TODO We should properly track this and invalidate existing TemporaryMemory blocks
    arena->tempCount = 0;

    if( clearToZero )
        PZERO( arena->base, arena->size );
}

inline sz
Available( const MemoryArena& arena )
{
    return arena.size - arena.used;
}

inline bool
IsInitialized( const MemoryArena& arena )
{
    return arena.base && arena.size;
}

inline MemoryParams
DefaultMemoryParams()
{
    MemoryParams result = {};
    result.flags = MemoryFlags_ClearToZero;
    result.alignment = 0;
    return result;
}

inline MemoryParams
NoClear()
{
    MemoryParams result = DefaultMemoryParams();
    result.flags &= ~MemoryFlags_ClearToZero;
    return result;
}

inline MemoryParams
Aligned( u32 alignment )
{
    MemoryParams result = DefaultMemoryParams();
    result.alignment = alignment;
    return result;
}

inline MemoryParams
Temporary()
{
    MemoryParams result = DefaultMemoryParams();
    result.flags |= MemoryFlags_TemporaryMemory;
    return result;
}

inline void *
_PushSize( MemoryArena *arena, sz size, MemoryParams params = DefaultMemoryParams() )
{
    ASSERT( arena->used + size <= arena->size );
    if( !(params.flags & MemoryFlags_TemporaryMemory) )
        ASSERT( arena->tempCount == 0 );

    void* free = arena->base + arena->used;
    void* result = free;
    sz waste = 0;

    if( params.alignment )
    {
        result = Align( free, params.alignment );
        waste = (u8*)result - (u8*)free;
    }

    arena->used += size + waste;

    if( params.flags & MemoryFlags_ClearToZero )
        PZERO( result, size );

    return result;
}

inline MemoryArena
MakeSubArena( MemoryArena* arena, sz size, MemoryParams params = DefaultMemoryParams() )
{
    MemoryArena result = {};
    result.base = (u8*)PUSH_SIZE( arena, size, params );
    result.size = size;

    return result;
}

struct TemporaryMemory
{
    MemoryArena *arena;
    sz usedRecord;
};

inline TemporaryMemory
BeginTemporaryMemory( MemoryArena *arena )
{
    TemporaryMemory result;

    result.arena = arena;
    result.usedRecord = arena->used;

    ++arena->tempCount;

    return result;
}

inline void
EndTemporaryMemory( TemporaryMemory& tempMem )
{
    MemoryArena *arena = tempMem.arena;

    ASSERT( arena->used >= tempMem.usedRecord );
    arena->used = tempMem.usedRecord;

    ASSERT( arena->tempCount > 0 );
    --arena->tempCount;
}

inline void
CheckTemporaryBlocks( MemoryArena *arena )
{
    ASSERT( arena->tempCount == 0 );
}



///// STATIC MEMORY POOL
// General memory pool of a fixed initial size, can allocate any object type or size
// Merges free contiguous blocks and searches linearly


enum MemoryBlockFlags : u32
{
    None = 0,
    Used = 0x01,
};

struct MemoryBlock
{
    MemoryBlock* prev;
    MemoryBlock* next;

    sz size;
    u32 flags;
};

inline MemoryBlock*
InsertBlock( MemoryBlock* prev, sz size, void* memory )
{
    // TODO 'size' includes the MemoryBlock struct itself for now
    // Are we sure we wanna do this??
    ASSERT( size > sizeof(MemoryBlock) );
    MemoryBlock* block = (MemoryBlock*)memory;
    // TODO Are we sure this shouldn't be the other way around??
    block->size = size - sizeof(MemoryBlock);
    block->flags = MemoryBlockFlags::None;
    block->prev = prev;
    block->next = prev->next;
    block->prev->next = block;
    block->next->prev = block;

    return block;
}

inline MemoryBlock*
FindBlockForSize( MemoryBlock* sentinel, sz size )
{
    MemoryBlock* result = nullptr;

    // TODO Best match block! (find smallest that fits)
    for( MemoryBlock* block = sentinel->next; block != sentinel; block = block->next )
    {
        if( block->size >= size && !(block->flags & MemoryBlockFlags::Used) )
        {
            result = block;
            break;
        }
    }

    return result;
}

inline void*
UseBlock( MemoryBlock* block, sz size, sz splitThreshold )
{
    ASSERT( size <= block->size );

    block->flags |= MemoryBlockFlags::Used;
    void* result = (block + 1);

    sz remainingSize = block->size - size;
    if( remainingSize > splitThreshold )
    {
        block->size -= remainingSize;
        InsertBlock( block, remainingSize, (u8*)result + size );
    }
    else
    {
        // TODO Record the unused portion so that it can be merged with neighbours
    }

    return result;
}

inline bool
MergeIfPossible( MemoryBlock* first, MemoryBlock* second, MemoryBlock* sentinel )
{
    bool result = false;

    if( first != sentinel && second != sentinel &&
        !(first->flags & MemoryBlockFlags::Used) &&
        !(second->flags & MemoryBlockFlags::Used) )
    {
        // This check only needed so we can support discontiguous memory pools if needed
        u8* expectedOffset = (u8*)first + sizeof(MemoryBlock) + first->size;
        if( (u8*)second == expectedOffset )
        {
            second->next->prev = second->prev;
            second->prev->next = second->next;

            first->size += sizeof(MemoryBlock) + second->size;

            result = true;
        }
    }

    return result;
}

// TODO Abstract the sentinel inside a 'MemoryPool' struct and put a pointer to that in the block
// so that we don't need to pass the sentinel, and we can remove the 'ownerPool' idea from the meshes
inline void
ReleaseBlockAt( void* memory, MemoryBlock* sentinel )
{
    MemoryBlock* block = (MemoryBlock*)memory - 1;
    block->flags &= ~MemoryBlockFlags::Used;

    MergeIfPossible( block, block->next, sentinel );
    MergeIfPossible( block->prev, block, sentinel );
}

#endif /* __MEMORY_H__ */
