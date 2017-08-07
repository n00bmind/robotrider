#ifndef __MEMORY_H__
#define __MEMORY_H__ 

struct MemoryArena
{
    u8 *base;
    mem_idx size;
    mem_idx used;

    u32 tempCount;
};

inline void
InitializeArena( MemoryArena *arena, u8 *base, mem_idx size )
{
    arena->base = base;
    arena->size = size;
    arena->used = 0;
    arena->tempCount = 0;
}

struct TemporaryMemory
{
    MemoryArena *arena;
    mem_idx used;
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

inline void *
_PushSize( MemoryArena *arena, mem_idx size )
{
    ASSERT( arena->used + size <= arena->size );
    // TODO Clear to zero option

    void *result = arena->base + arena->used;
    arena->used += size;
    return result;
}

#endif /* __MEMORY_H__ */
