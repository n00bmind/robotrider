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

#ifndef __DATA_TYPES_H__
#define __DATA_TYPES_H__ 

// FIXME Get rid of these
#include <mutex>
#include <condition_variable>

/////     DYNAMIC ARRAY    /////

template <typename T>
struct Array
{
    u32 count;
    u32 maxCount;
    T *data;

    Array( MemoryArena* arena, u32 maxCount_ )
    {
        data = PUSH_ARRAY( arena, maxCount_, T );
        count = 0;
        maxCount = maxCount_;
    }

    Array( T* data_, u32 maxCount_ )
    {
        data = data_;
        count = 0;
        maxCount = maxCount_;
    }

    T& operator[]( u32 i )
    {
        ASSERT( i < maxCount );
        return data[i];
    }

    const T& operator[]( u32 i ) const
    {
        ASSERT( i < maxCount );
        return data[i];
    }

    // TODO Not too sure about this!
    operator bool() const
    {
        return data != nullptr;
    }

    T* Reserve()
    {
        ASSERT( count < maxCount );
        return data + count++;
    }

    void Add( const T& item )
    {
        T* slot = Reserve();
        *slot = item;
    }

    void Push( const T& item )
    {
        Add( item );
    }

    T Pop()
    {
        ASSERT( count > 0 );
        --count;
        return data[count];
    }

    void BlitTo( T* buffer ) const
    {
        memcpy( buffer, data, count * sizeof(T) );
    }

    void Clear()
    {
        count = 0;
    }

protected:

    Array()
    {
        data = nullptr;
        count = 0;
        maxCount = 0;
    }
};

#define ARRAY(type, count, name) type _##name[count];Array<type> name( _##name, count );

template <typename T>
struct Array2
{
    u32 rows;
    u32 cols;
    T *data;

    Array2( MemoryArena* arena, u32 rows_, u32 cols_ )
    {
        rows = rows_;
        cols = cols_;
        data = PUSH_ARRAY( arena, rows * cols, T );
    }

    T& At( u32 r, u32 c )
    {
        ASSERT( r < rows );
        ASSERT( c < cols );
        return data[r * cols + c];
    }

    const T& At( u32 r, u32 c ) const
    {
        ASSERT( r < rows );
        ASSERT( c < cols );
        return data[r * cols + c];
    }
};


/////     HASH TABLE    /////

// NOTE Type K must support == comparison
template <typename K, typename V, u32 (*H)( const K&, u32 )>
struct HashTable
{
    struct Slot
    {
        bool occupied;
        K key;
        V value;
        Slot* nextInHash;
    };


    Slot* table;
    u32 tableSize;
    u32 count;

    HashTable( MemoryArena* arena, u32 size )
    {
        table = PUSH_ARRAY( arena, size, Slot );
        tableSize = size;
        count = 0;

        Clear();
    }

    void Clear()
    {
        for( u32 i = 0; i < tableSize; ++i )
            table[i].occupied = false;
        // FIXME Add existing externally chained elements to a free list like in BucketArray
        count = 0;
    }

    u32 IndexFromKey( const K& key )
    {
        u32 hashValue = H( key, tableSize );
        u32 result = hashValue % tableSize;
        return result;
    }

    V* Find( const K& key )
    {
        u32 idx = IndexFromKey( key );

        Slot* slot = &table[idx];
        if( slot->occupied )
        {
            do
            {
                if( slot->key == key )
                    return &slot->value;

                slot = slot->nextInHash;
            } while( slot );
        }

        return nullptr;
    }

#if 0
    V& operator[]( const K& key )
    {
        return *Find( key );
    }
#endif

    V* Reserve( const K& key, MemoryArena* arena )
    {
        u32 idx = IndexFromKey( key );

        Slot* prev = nullptr;
        Slot* slot = &table[idx];
        if( slot->occupied )
        {
            do
            {
                // TODO Allow key comparisons different from bit-equality if needed
                if( slot->key == key )
                    return nullptr;

                prev = slot;
                slot = slot->nextInHash;
            } while( slot );

            slot = PUSH_STRUCT( arena, Slot );
            prev->nextInHash = slot;
        }

        count++;
        slot->occupied = true;
        slot->key = key;
        slot->nextInHash = nullptr;
        ZERO( &slot->value, sizeof(V) );

        return &slot->value;
    }

    bool Add( const K& key, const V& value, MemoryArena* arena )
    {
        bool result = false;

        V* slotValue = Reserve( key, arena );
        if( slotValue )
        {
            *slotValue = value;
            result = true;
        }

        return result;
    }

    Array<K> Keys( MemoryArena* arena ) const
    {
        Array<K> result( arena, count );
        for( u32 i = 0; i < tableSize; ++i )
        {
            if( table[i].occupied )
            {
                result.Add( table[i].key );
                if( table[i].nextInHash )
                {
                    Slot* slot = table[i].nextInHash;
                    while( slot )
                    {
                        result.Add( slot->key );
                        slot = slot->nextInHash;
                    }
                }
            }
        }
        return result;
    }

    Array<V> Values( MemoryArena* arena ) const
    {
        Array<V> result( arena, count );
        for( u32 i = 0; i < tableSize; ++i )
        {
            if( table[i].occupied )
            {
                result.Add( table[i].value );
                if( table[i].nextInHash )
                {
                    Slot* slot = table[i].nextInHash;
                    while( slot )
                    {
                        result.Add( slot->value );
                        slot = slot->nextInHash;
                    }
                }
            }
        }
        return result;
    }

private:
    
    // Disallow implicit copying
    HashTable( const HashTable& );
    HashTable& operator =( const HashTable& );
};

/////     STRING     /////

// NOTE We're using stdlib but still wrapping everything in case we wanna do something different in the future
inline u32
Length( const char* cstring )
{
    return (u32)strlen( cstring );
}

struct String
{
    u32 size;
    u32 maxSize;
    char *data;

    // NOTE For now we don't allow (const) string literals because that's readonly memory. We'll see..
    String( char *cString )
    {
        data = cString;
        size = maxSize = Length( cString );
    }

    String( char *cString, u32 size_, u32 maxSize_ )
    {
        data = cString;
        size = size_;
        maxSize = maxSize_;
    }

    bool IsNullOrEmpty() const
    {
        return data == nullptr || *data == '\0' || size == 0;
    }

    bool IsEqual( const char* cString ) const
    {
        return strncmp( data, cString, size ) == 0
            && cString[size] == '\0';
    }

    bool IsNewline() const
    {
        return *data == '\n' || (*data == '\r' && *(data + 1) == '\n');
    }

    bool StartsWith( const char* cString )
    {
        ASSERT( *cString );
        if( IsNullOrEmpty() )
            return true;

        char* nextThis = data;
        const char* nextThat = cString;

        while( *nextThis && *nextThis == *nextThat )
        {
            nextThis++;
            nextThat++;

            if( *nextThat == '\0' )
                return true;
        }

        return false;
    }

    char* FindString( const char* cString )
    {
        ASSERT( *cString );
        if( IsNullOrEmpty() )
            return nullptr;

        char* prospect = data;
        u32 remaining = size;
        while( remaining && *prospect )
        {
            char* nextThis = prospect;
            const char* nextThat = cString;
            while( *nextThis && *nextThis == *nextThat )
            {
                nextThis++;
                nextThat++;

                if( *nextThat == '\0' )
                    return prospect;
            }

            prospect++;
            remaining--;
        }

        return nullptr;
    }

    String ConsumeLine()
    {
        u32 lineLen = size;

        char* onePastNL = FindString( "\n" );

        if( onePastNL )
        {
            onePastNL++;
            lineLen = SafeTruncToU32( onePastNL - data );
        }

        String line( data, lineLen, maxSize );

        ASSERT( lineLen <= size );
        data = onePastNL;
        size -= lineLen;
        maxSize -= lineLen;

        return line;
    }

    // Consume next word trimming whatever whitespace is there at the beginning
    String ConsumeWord()
    {
        char *start = data;
        u32 remaining = size;

        // TODO We may not want things like '\n' here?
        while( *start && isspace( *start ) && remaining > 0 )
        {
            start++;
            remaining--;
        }

        u32 trimmed = SafeTruncToU32( start - data );
        u32 wordLen = 0;
        if( *start && remaining > 0 )
        {
            char *end = start;
            while( *end && !isspace( *end ) )
                end++;

            wordLen = SafeTruncToU32( end - start );
        }

        String result( start, wordLen, maxSize - trimmed );

        // Consume stripped part
        u32 totalLen = trimmed + wordLen;
        ASSERT( totalLen <= size );
        data += totalLen;
        size -= totalLen;
        maxSize -= totalLen; 

        return result;
    }

    int Scan( const char *format, ... )
    {
        va_list args;
        va_start( args, format );

        // HACK Not pretty, but it's what we have until C gets a portable bounded vscanf
        char endChar = *(data + size);
        *(data + size) = '\0';
        int result = vsscanf( data, format, args );
        *(data + size) = endChar;

        va_end( args );
        return result;
    }

    operator bool() const
    {
        return !IsNullOrEmpty();
    }
};


/////     BUCKET ARRAY     /////

template <typename T>
struct BucketArray
{
    struct Bucket
    {
        T* data;
        u32 size;

        u32 count;

        Bucket *next;
        Bucket *prev;

        Bucket( u32 size_, MemoryArena* arena )
        {
            data = PUSH_ARRAY( arena, size_, T );
            size = size_;
            count = 0;
            next = prev = nullptr;
        }

        void Clear()
        {
            count = 0;
            next = prev = nullptr;
        }
    };

    struct Idx
    {
        Bucket* base;
        u32 index;

        operator bool() const
        {
            return IsValid();
        }

        operator T&() const
        {
            ASSERT( IsValid() );
            return base->data[index];
        }

        bool IsValid() const
        {
            return base && index < base->count;
        }

        void Next()
        {
            if( index < base->count - 1 )
                index++;
            else
            {
                base = base->next;
                index = 0;
            }
        }

        void Prev()
        {
            if( index > 0 )
                index--;
            else
            {
                base = base->prev;
                index = base ? base->count - 1 : 0;
            }
        }
    };


    Bucket first;
    Bucket* last;
    Bucket* firstFree;
    u32 count;

    MemoryArena* arena;


    BucketArray( u32 bucketSize, MemoryArena* arena )
        : first( bucketSize, arena )
    {
        count = 0;
        last = &first;
        firstFree = nullptr;
        this->arena = arena;
    }

    T* Reserve()
    {
        if( last->count == last->size )
            AddEmptyBucket();

        count++;
        return &last->data[last->count++];
    }

    void Add( const T& item )
    {
        T* slot = Reserve();
        *slot = item;
    }

    void Remove( const Idx& index )
    {
        ASSERT( index.IsValid() );

        // If index is not pointing to last item, find last item and swap it
        if( index.base != last || index.index != last->count - 1 )
        {
            T& lastItem = last->data[last->count - 1];
            (T&)index = lastItem;
        }

        last->count--;
        if( last->count == 0 && last != &first )
        {
            // Empty now, so place it at the beginning of the free list
            last->next = firstFree;
            firstFree = last;

            last = last->prev;
        }

        count--;
    }

    void BlitTo( T* buffer ) const
    {
        const Bucket* bucket = &first;
        while( bucket )
        {
            memcpy( buffer, bucket->data, bucket->count * sizeof(T) );
            buffer += bucket->count;
            bucket = bucket->next;
        }
    }

    void Clear()
    {
        if( last != &first )
        {
            // Place all chained buckets in the free list
            last->next = firstFree;
            firstFree = first.next;
        }

        first.Clear();
        count = 0;
        last = &first;
    }

    Idx First()
    {
        return { &first, 0 };
    }

    Idx Last()
    {
        return { last, last->count - 1 };
    }

    T& operator[]( const Idx& idx )
    {
        ASSERT( idx.IsValid() );
        return (T&)idx;
    }

    const T& operator[]( const Idx& idx ) const
    {
        return (*this)[idx];
    }

    T& operator[]( u32 i )
    {
        ASSERT( i < count );

        Bucket* base = &first;
        u32 index = i;

        while( index >= base->count )
        {
            index -= base->count;
            base = base->next;
        }

        return base->data[index];
    }

    const T& operator[]( u32 i ) const
    {
        return (*this)[i];
    }

private:
    
    // Disallow implicit copying
    BucketArray( const BucketArray& );
    BucketArray& operator =( const BucketArray& );

    void AddEmptyBucket()
    {
        Bucket* newBucket;
        if( firstFree )
        {
            newBucket = firstFree;
            firstFree = firstFree->next;
            newBucket->Clear();
        }
        else
        {
            newBucket = PUSH_STRUCT( arena, Bucket );
            *newBucket = Bucket( first.size, arena );
        }
        newBucket->prev = last;
        last->next = newBucket;

        last = newBucket;
    }
};


/////     (intrusive) LINKED LIST    /////

template <typename T>
struct LinkedList
{
    u32 count;
    // Dummy value so that the list always has something in it, and we don't have to check
    // for that edge case when inserting/removing (and we never have nullptrs!)
    T sentinel;

    // Horrible C++ 11 shit
    using TN = decltype(sentinel.next);
    static_assert( std::is_pointer<TN>::value && std::is_base_of< T, typename std::remove_pointer<TN>::type >::value,
                   "Type needs to declare a pointer to itself called 'next'" );
    using TP = decltype(sentinel.prev);
    static_assert( std::is_pointer<TP>::value && std::is_base_of< T, typename std::remove_pointer<TP>::type >::value,
                   "Type needs to declare a pointer to itself called 'prev'" );

    LinkedList()
    {
        sentinel = {};
        sentinel.next = &sentinel;
        sentinel.prev = &sentinel;
        count = 0;
    }

    void Remove( T* item )
    {
        ASSERT( item->next && item->prev );

        item->prev->next = item->next;
        item->next->prev = item->prev;
        count--;

        item->next = item->prev = nullptr;
    }

    void PushFront( T* item )
    {
        item->prev = &sentinel;
        item->next = sentinel.next;

        item->next->prev = item;
        item->prev->next = item;

        count++;
    }

    T* PopFront()
    {
        T* result = nullptr;
        if( sentinel.next != &sentinel )
        {
            result = sentinel.next;
            Remove( result );
        }
        return result;
    }

    void PushBack( T* item )
    {
        item->next = &sentinel;
        item->prev = sentinel.prev;

        item->next->prev = item;
        item->prev->next = item;

        count++;
    }

    T* PopBack()
    {
        T* result = nullptr;
        if( sentinel.prev != &sentinel )
        {
            result = sentinel.prev;
            Remove( result );
        }
        return result;
    }

private:
    
    // Disallow implicit copying
    LinkedList( const LinkedList& );
    LinkedList& operator =( const LinkedList& );
};


/////     QUEUE     /////
// TODO Test!

template <typename T>
struct Queue
{
    LinkedList<T> list;

    void Push( T* item )
    {
        list.PushBack( item );
    }

    T* Pop()
    {
        return list.PopFront();
    }

    bool IsEmpty()
    {
        return list.count == 0;
    }
};


/////     CONCURRENT QUEUE     /////
// TODO Reimplement using ticket-taking
// (https://hero.handmade.network/episode/code/day325/)
// TODO Test!

template <typename T>
struct ConcurrentQueue
{
    Queue<T> queue;

    std::mutex mutex;
    std::condition_variable condition;

    void Push( T* item )
    {
        std::unique_lock<std::mutex> lock(mutex);

        queue.Push( item );
        lock.unlock();

        condition.notify_one();
    }

    bool TryPop( T** destItem )
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        if( queue.IsEmpty() )
            return false;

        *destItem = queue.Pop();
        return true;
    }

    T* PopOrBlock()
    {
        std::unique_lock<std::mutex> lock(mutex);
        
        while( queue.IsEmpty() )
            condition.wait( lock );

        return queue.Pop();
    }

    bool IsEmpty()
    {
        std::unique_lock<std::mutex> lock(mutex);

        return queue.IsEmpty();
    }
};




/////     TESTS     /////

struct Dummy
{
    Dummy *next;
    Dummy *prev;
};

void TestDataTypes()
{
    // TODO 
#if 0
    ConcurrentQueue<Dummy> q;

    // This now requires a memory arena for construction
    //BucketArray<float> bkArray;
    //Array<v3> dynArray;
    SArray<v3, 10> stArray;
    v3 aLotOfVecs[10];

    //dynArray.data = aLotOfVecs;
    //dynArray.maxCount = ARRAYCOUNT(aLotOfVecs);
#endif
}

#endif /* __DATA_TYPES_H__ */
