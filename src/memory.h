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



struct MemoryArena
{
    u8 *base;
    sz size;
    sz used;

    u32 tempCount;
};

inline void
InitializeArena( MemoryArena *arena, u8 *base, sz size )
{
    arena->base = base;
    arena->size = size;
    arena->used = 0;
    arena->tempCount = 0;
}

struct TemporaryMemory
{
    MemoryArena *arena;
    sz used;
};

inline TemporaryMemory
BeginTemporaryMemory( MemoryArena *arena )
{
    TemporaryMemory result;

    result.arena = arena;
    result.used = arena->used;

    ++arena->tempCount;

    return result;
}

inline void
EndTemporaryMemory( TemporaryMemory tempMem )
{
    MemoryArena *arena = tempMem.arena;

    ASSERT( arena->used >= tempMem.used );
    arena->used = tempMem.used;

    ASSERT( arena->tempCount > 0 );
    --arena->tempCount;
}

inline void
CheckArena( MemoryArena *arena )
{
    ASSERT( arena->tempCount == 0 );
}

#define PUSH_STRUCT(arena, type) (type *)_PushSize( arena, sizeof(type) )
#define PUSH_ARRAY(arena, count, type) (type *)_PushSize( arena, (count)*sizeof(type) )
#define PUSH_SIZE(arena, size) _PushSize( arena, size )

#define PUSH_ARRAY_ALIGNED(arena, count, type, alignment) (type *)_PushSize( arena, (count)*sizeof(type), alignment )
#define PUSH_SIZE_ALIGNED(arena, size, alignment) _PushSize( arena, size, alignment )

inline void *
_PushSize( MemoryArena *arena, sz size )
{
    ASSERT( arena->used + size <= arena->size );
    // TODO Clear to zero option

    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

inline void *
_PushSize( MemoryArena *arena, sz size, sz alignment )
{
    ASSERT( arena->used + size <= arena->size );
    // TODO Clear to zero option

    void *free = arena->base + arena->used;
    void* result = Align( free, alignment );
    sz waste = (u8*)result - (u8*)free;

    arena->used += size + waste;
    return result;
}

#endif /* __MEMORY_H__ */
