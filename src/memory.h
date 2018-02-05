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
#define PUSH_SIZE(arena, size) _PushSize( arena, size )
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
