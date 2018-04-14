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
    T *data;
    u32 count;
    u32 maxCount;

    Array( MemoryArena* arena, u32 maxCount_ )
    {
        data = PUSH_ARRAY( arena, maxCount_, T );
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

    operator bool() const
    {
        return data != nullptr;
    }

    void Add( const T& item )
    {
        ASSERT( count < maxCount );
        data[count++] = item;
    }

    T* Place()
    {
        ASSERT( count < maxCount );
        return data + count++;
    }

    void BlitTo( T* buffer ) const
    {
        memcpy( buffer, data, count * sizeof(T) );
    }

protected:

    Array()
    {
        data = nullptr;
        count = 0;
        maxCount = 0;
    }
};

/////     STATIC ARRAY    /////

template <typename T, u32 N>
struct SArray : public Array<T>
{
    T storage[N];

    SArray() : Array()
    {
        data = storage;
        count = 0;
        maxCount = N;
    }
};

/////     HASH TABLE    /////

template <typename K, typename T>
struct HashTable
{
    typedef u32 HashFunction( const K& key, u32 bitCount );

    struct HashSlot
    {
        bool filled;
        K key;
        T value;
        HashSlot* nextInHash;
    };

    Array<HashSlot> array;
    u32 count;
    HashFunction* hashFunction;

    HashTable( MemoryArena* arena, u32 maxCount_, HashFunction* hashFunction_ )
    {
        // FIXME Check maxCount_ is a power of 2
        //ASSERT( maxCount_ && ((maxCount_ & (maxCount_ - 1) == 0) );

        //array.Init( arena, maxCount_ );
        array = Array( arena, maxCount_ );
        // The internal array is always marked as filled so we can lookup/store
        // at arbitrary indices
        array.count = maxCount_;
        hashFunction = hashFunction_;

        Clear();
    }

    void Clear()
    {
        for( int i = 0; i < array.maxCount; ++i )
            array[i].filled = false;
        // TODO Deallocate externally chained elements if we ever end up supporting that
        count = 0;
    }

    u32 IndexFromKey( const K& key )
    {
        u32 hashValue = hashFunction( key );
        u32 result = hashValue & (array.maxCount - 1);
        return result;
    }

    T* Find( const K& key )
    {
        u32 idx = IndexFromKey( key );

        HashSlot<T>* slot = &array[idx];
        if( slot->filled )
        {
            do
            {
                // TODO Allow key comparisons different from bit-equality if needed
                if( slot->key == key )
                    return slot;

                slot = slot->nextInHash;
            } while( slot );
        }

        return nullptr;
    }

    void Add( const K& key, const T& value, MemoryArena* arena )
    {
        u32 idx = IndexFromKey( key );

        HashSlot<T>* slot = &array[idx];
        if( slot->filled )
        {
            do
            {
                // TODO Allow key comparisons different from bit-equality if needed
                if( slot->key == key )
                    break;

                slot = slot->nextInHash;
            } while( slot );

            slot = PUSH_STRUCT( arena, T );
        }

        *slot = { true, key, value, nullptr };
        count++;
    }
};

// TODO N _must_ be a power of 2!
template <typename K, typename T, u32 N>
struct SHashTable : public HashTable<K, T>
{
    HashSlot storage[N];

    SHashTable( MemoryArena* arena, HashFunction* hashFunction_ )
        : HashTable( arena, hashFunction_ )
    {
        array.data = storage;
        array.count = N;
        array.maxCount = N;
    }
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
    char *data;
    u32 size;
    u32 maxSize;

    // NOTE For now we don't allow (const) string literals,
    // otherwise we should make our own data const too. We'll see..
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

    bool IsNullOrEmpty()
    {
        return data == nullptr || *data == '\0';
    }

    bool IsEqual( const char* cString )
    {
        return strncmp( data, cString, size ) == 0
            && cString[size] == '\0';
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

        // HACK Horrible, but it's what we have for now
        char endChar = *(data + size);
        *(data + size) = '\0';
        int result = vsscanf( data, format, args );
        *(data + size) = endChar;

        va_end( args );
        return result;
    }
};

// TODO
/////     BUCKET ARRAY     /////

template <typename T, u32 N = 8>
struct Bucket
{
    T data[N];
    bool occupied[N];

    u32 count;
    u32 bucketIndex;

    Bucket *next;
    Bucket *prev;
};

// TODO Implement using LinkedList?
template <typename T, u32 N = 8>
struct BucketArray
{
    u32 count;
    Bucket<T, N> first;
    Bucket<T, N> *last; // ?
    // TODO Keep buckets compact when deleting items
    // TODO Keep totally empty buckets in their own separate list for reusing later
    Bucket<T, N>* firstFree;
};

//template <typename T, u32 N = 8>
//struct BucketArray
//{
    //u32 count;
    //Bucket<T, N> *data;
//};


/////     LINKED LIST    /////

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
// TODO Test!
// TODO Speed up using (lock-free) intrinsics and get rid of the mutex/condition variable

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
    ConcurrentQueue<Dummy> q;
    Bucket<float> bucket;

    Array<v3> dynArray;
    SArray<v3, 10> stArray;
    v3 aLotOfVecs[10];

    dynArray.data = aLotOfVecs;
    dynArray.maxCount = ARRAYCOUNT(aLotOfVecs);
}

#endif /* __DATA_TYPES_H__ */
