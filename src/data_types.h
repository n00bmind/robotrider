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

    Array()
    {
        data = nullptr;
        count = 0;
        maxCount = 0;
    }

    Array( MemoryArena* arena, u32 count_, u32 maxCount_ )
    {
        data = PUSH_ARRAY( arena, maxCount_, T );
        count = count_;
        maxCount = maxCount_;
    }

    Array( T* data_, u32 count_, u32 maxCount_ )
    {
        data = data_;
        count = count_;
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

    T* PushEmpty( bool clear = true )
    {
        ASSERT( count < maxCount );
        T* result = data + count++;
        if( clear )
            ZERO( result, sizeof(T) );

        return result;
    }

    T* Push( const T& item )
    {
        T* slot = PushEmpty( false );
        *slot = item;

        return slot;
    }

    T Pop()
    {
        ASSERT( count > 0 );
        --count;
        return data[count];
    }

    // Deep copy
    Array<T> Copy( MemoryArena* arena ) const
    {
        Array<T> result = Array<T>( arena, count );
        COPY( data, result.data, count * sizeof(T) );
        result.count = count;
        return result;
    }

    void CopyTo( Array<T>* out ) const
    {
        ASSERT( out->maxCount >= count );
        COPY( data, out->data, count * sizeof(T) );
        out->count = count;
    }

    void CopyTo( T* buffer ) const
    {
        COPY( data, buffer, count * sizeof(T) );
    }

    void CopyFrom( const T* buffer, u32 count_ )
    {
        count = count_;
        COPY( buffer, data, count * sizeof(T) );
    }

    void Clear()
    {
        count = 0;
    }

    u32 Available()
    {
        return maxCount - count;
    }
};

#define ARRAY(type, count, name) type _##name[count];Array<type> name( _##name, 0, count );


template <typename T>
struct Array2
{
    u32 rows;
    u32 cols;
    T *data;

    Array2()
    {
        rows = 0;
        cols = 0;
        data = nullptr;
    }

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

    T* AtRow( u32 r )
    {
        ASSERT( r < rows );
        return &data[r * cols];
    }

    const T* AtRow( u32 r ) const
    {
        ASSERT( r < rows );
        return &data[r * cols];
    }

    // TODO Not too sure about this!
    operator bool() const
    {
        return data != nullptr;
    }

    // Deep copy
    void CopyTo( Array2<T>* out ) const
    {
        ASSERT( rows == out->rows );
        ASSERT( cols == out->cols );
        COPY( data, out->data, rows * cols * sizeof(T) );
    }
};


/////     RING BUFFER    /////

template <typename T>
struct RingBuffer
{
    Array<T> buffer;
    u32 headIndex;


    RingBuffer( MemoryArena* arena, u32 maxCount_ )
        : buffer( arena, 0, maxCount_ )
    {
        headIndex = 0;
    }

    u32 Count() const
    {
        return buffer.count;
    }

    T* PushEmpty( bool clear = true )
    {
        T* result = buffer.data + headIndex++;
        if( headIndex == buffer.maxCount )
            headIndex = 0;
        if( buffer.count < buffer.maxCount )
            buffer.count++;
        if( clear )
            ZERO( result, sizeof(T) );

        return result;
    }

    T* Push( const T& item )
    {
        T* result = PushEmpty( false );
        *result = item;
        return result;
    }

    bool Contains( const T& item ) const
    {
        bool result = false;
        for( u32 i = 0; i < buffer.count; ++i )
        {
            if( buffer.data[i] == item )
            {
                result = true;
                break;
            }
        }

        return result;
    }
};

/////     RING STACK    /////
// Circular stack that can push an infinite number of elements, by discarding elements from the bottom when needed

template <typename T>
struct RingStack
{
    Array<T> buffer;
    u32 topIndex;


    RingStack( MemoryArena* arena, u32 maxCount_ )
        : buffer( arena, 0, maxCount_ )
    {
        topIndex = 0;
    }

    u32 Count() const
    {
        return buffer.count;
    }

    T* Top()
    {
        T* result = nullptr;
        if( buffer.count )
        {
            u32 top = topIndex ? topIndex - 1 : buffer.maxCount - 1;
            result = buffer.data + top;
        }
        return result;
    }

    // Return the item that is at the given distance from the top (0 equals the top item)
    const T& At( u32 fromTop ) const
    {
        ASSERT( fromTop < buffer.count );
        u32 index = (topIndex - fromTop - 1) % buffer.maxCount;

        return buffer[index];
    }

    T* PushEmpty( bool clear = true )
    {
        T* result = buffer.data + topIndex++;
        if( topIndex == buffer.maxCount )
            topIndex = 0;
        if( buffer.count < buffer.maxCount )
            buffer.count++;

        if( clear )
            ZERO( result, sizeof(T) );

        return result;
    }

    T* Push( const T& item )
    {
        T* result = PushEmpty( false );
        *result = item;
        return result;
    }

    T Pop()
    {
        if( buffer.count )
        {
            topIndex = topIndex ? --topIndex : buffer.maxCount - 1;
            --buffer.count;
        }
        else
        {
            INVALID_CODE_PATH
        }

        return buffer.data[topIndex];
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

    // Disallow implicit copying
    HashTable( const HashTable& ) = delete;
    HashTable& operator =( const HashTable& ) = delete;

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
        Array<K> result( arena, 0, count );
        for( u32 i = 0; i < tableSize; ++i )
        {
            if( table[i].occupied )
            {
                result.Push( table[i].key );
                if( table[i].nextInHash )
                {
                    Slot* slot = table[i].nextInHash;
                    while( slot )
                    {
                        result.Push( slot->key );
                        slot = slot->nextInHash;
                    }
                }
            }
        }
        return result;
    }

    Array<V> Values( MemoryArena* arena ) const
    {
        Array<V> result( arena, 0, count );
        for( u32 i = 0; i < tableSize; ++i )
        {
            if( table[i].occupied )
            {
                result.Push( table[i].value );
                if( table[i].nextInHash )
                {
                    Slot* slot = table[i].nextInHash;
                    while( slot )
                    {
                        result.Push( slot->value );
                        slot = slot->nextInHash;
                    }
                }
            }
        }
        return result;
    }
};

/////     STRING     /////

// NOTE We're using stdlib but still wrapping everything in case we wanna do something different in the future
inline u32
Length( const char* cstring )
{
    return (u32)strlen( cstring );
}

inline bool
IsWhitespace( char c )
{
    return c == ' ' || c == '\t';
}

inline bool
IsWord( char c )
{
    return c >= 32 && c < 127 && !IsWhitespace( c );
}

// Read-only string
// Covers most C-string use cases
// Specially useful for fast zero-copy reads from text files, etc.
struct String
{
    u32 size;
    u32 maxSize;
    const char *data;

    String()
    {
        data = nullptr;
        size = maxSize = 0;
    }

    String( const char *cString )
    {
        data = cString;
        size = maxSize = Length( cString );
    }

    String( const char *cString, u32 size_, u32 maxSize_ )
    {
        data = cString;
        size = size_;
        maxSize = maxSize_;
    }

    const String& operator =( const char* cString )
    {
        data = cString;
        size = maxSize = Length( cString );

        return *this;
    }

    // Deep copy
    void CopyTo( String* target, MemoryArena* arena ) const
    {
        target->size = size;
        target->maxSize = size;
        target->data = PUSH_ARRAY( arena, size, char );
        COPY( data, (char*)target->data, size * sizeof(char) );
    }

    const char* CString( MemoryArena* arena ) const
    {
        char* result = PUSH_ARRAY( arena, size + 1, char );
        COPY( data, result, size * sizeof(char) );

        return result;
    }

    bool IsNullOrEmpty() const
    {
        return data == nullptr || *data == '\0' || size == 0;
    }

    operator bool() const
    {
        return !IsNullOrEmpty();
    }

    bool IsEqual( const char* cString, bool caseSensitive = true ) const
    {
        if( IsNullOrEmpty() )
            return false;

        bool result = false;

        if( caseSensitive )
            result = strncmp( data, cString, size ) == 0 && cString[size] == '\0';
        else
        {
            const char* nextThis = data;
            const char* nextThat = cString;

            while( *nextThis && tolower( *nextThis ) == tolower( *nextThat ) )
            {
                nextThis++;
                nextThat++;

                if( *nextThat == '\0' )
                {
                    result = true;
                    break;
                }
            }
        }

        return result;
    }

    bool operator ==( const char* cString ) const
    {
        return IsEqual( cString );
    }

    bool IsNewline() const
    {
        return *data == '\n' || (*data == '\r' && *(data + 1) == '\n');
    }

    bool StartsWith( const char* cString ) const
    {
        ASSERT( *cString );
        if( IsNullOrEmpty() )
            return false;

        const char* nextThis = data;
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

    const char* FindString( const char* cString ) const
    {
        ASSERT( *cString );
        if( IsNullOrEmpty() )
            return nullptr;

        const char* prospect = data;
        u32 remaining = size;
        while( remaining && *prospect )
        {
            const char* nextThis = prospect;
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

        const char* atNL = data + size;
        const char* onePastNL = FindString( "\n" );

        if( onePastNL )
        {
            atNL = onePastNL;
            onePastNL++;
            // Account for stupid Windows "double" NLs
            if( *(atNL - 1) == '\r' )
                atNL--;

            lineLen = SafeU64ToU32( onePastNL - data );
        }

        ASSERT( lineLen <= size );
        String line( data, SafeU64ToU32( atNL - data ), maxSize );

        data = onePastNL;
        size -= lineLen;
        maxSize -= lineLen;

        return line;
    }

    u32 ConsumeWhitespace()
    {
        const char *start = data;
        u32 remaining = size;

        while( *start && IsWhitespace( *start ) && remaining > 0 )
        {
            start++;
            remaining--;
        }

        u32 advanced = size - remaining;

        data = start;
        size = remaining;
        maxSize -= advanced;

        return advanced;
    }

    // Consume next word trimming whatever whitespace is there at the beginning
    String ConsumeWord()
    {
        ConsumeWhitespace();

        u32 wordLen = 0;
        if( *data && size > 0 )
        {
            const char *end = data;
            while( *end && IsWord( *end ) )
                end++;

            wordLen = SafeU64ToU32( end - data );
        }

        String result( data, wordLen, maxSize );

        // Consume stripped part
        ASSERT( wordLen <= size );
        data += wordLen;
        size -= wordLen;
        maxSize -= wordLen; 

        return result;
    }

    String Consume( u32 charCount )
    {
        const char *next = data;
        u32 remaining = Min( charCount, size );

        while( *next && remaining > 0 )
        {
            next++;
            remaining--;
        }

        u32 len = SafeU64ToU32( next - data );
        String result( data, len, maxSize );

        data = next;
        size -= len;
        maxSize -= len;

        return result;
    }

    int Scan( const char *format, ... )
    {
        va_list args;
        va_start( args, format );

        // HACK Not pretty, but it's what we have until C gets a portable bounded vscanf
        char* end = (char*)data + size;
        char endChar = *end;
        *end = '\0';
        int result = vsscanf( data, format, args );
        *end = endChar;

        va_end( args );
        return result;
    }

    String ScanString()
    {
        String result;

        ConsumeWhitespace();

        const char* start = data;
        if( *start == '\'' || *start == '"' )
        {
            const char* next = start + 1;
            u32 remaining = size - 1;

            while( *next && remaining > 0 )
            {
                if( *next == *start && *(next - 1) != '\\' )
                    break;

                next++;
                remaining--;
            }

            ++start;
            result.data = start;
            result.size = SafeU64ToU32(next - start);
            result.maxSize = maxSize;
        }

        return result;
    }

    bool ToI32( i32* output )
    {
        bool result = false;

        char start = data[0];
        if( start == '+' || start == '-' )
            start = data[1];

        if( start >= '0' && start <= '9' )
        {
            char* end = (char*)1;
            *output = strtol( data, &end, 0 );

            if( *output == 0 )
                result = (end != (char*)1);
            else if( *output == LONG_MAX || *output == LONG_MIN )
                result = (errno != ERANGE);
            else
                result = true;
        }

        return result;
    }

    bool ToU32( u32* output )
    {
        bool result = false;

        char start = data[0];
        if( start >= '0' && start <= '9' )
        {
            char* end = (char*)1;
            *output = strtoul( data, &end, 0 );

            if( *output == 0 )
                result = (end != (char*)1);
            else if( *output == ULONG_MAX )
                result = (errno != ERANGE);
            else
                result = true;
        }

        return result;
    }

    bool ToBool( bool* output )
    {
        bool result = false;
        if( IsEqual( "true", false ) || IsEqual( "y", false ) || IsEqual( "yes", false ) || IsEqual( "1" ) )
        {
            result = true;
            *output = true;
        }
        else if( IsEqual( "false", false ) || IsEqual( "n", false ) || IsEqual( "no", false ) || IsEqual( "0" ) )
        {
            result = true;
            *output = false;
        }

        return result;
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

        Bucket( MemoryArena* arena, u32 size_ )
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


    BucketArray( MemoryArena* arena, u32 bucketSize )
        : first( arena, bucketSize )
    {
        count = 0;
        last = &first;
        firstFree = nullptr;
        this->arena = arena;
    }

    // Disallow implicit copying
    BucketArray( const BucketArray& ) = delete;
    BucketArray& operator =( const BucketArray& ) = delete;

    T* Reserve()
    {
        if( last->count == last->size )
            AddEmptyBucket();

        count++;
        return &last->data[last->count++];
    }

    T* Add( const T& item )
    {
        T* slot = Reserve();
        *slot = item;
        return slot;
    }

    T Remove( const Idx& index )
    {
        ASSERT( count > 0 );
        ASSERT( index.IsValid() );

        T result = (T&)index;
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
        return result;
    }

    T Pop()
    {
        T result = Remove( { last, last->count - 1 } );
        return result;
    }

    void CopyTo( T* buffer ) const
    {
        const Bucket* bucket = &first;
        while( bucket )
        {
            COPY( bucket->data, buffer, bucket->count * sizeof(T) );
            buffer += bucket->count;
            bucket = bucket->next;
        }
    }

    Array<T> ToArray( MemoryArena* arena_ ) const
    {
        Array<T> result( arena_, count, count );
        CopyTo( result.data );
        result.count = count;

        return result;
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
            *newBucket = Bucket( arena, first.size );
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
    // Convenience struct that can be embedded in user types
    // TODO Provide all methods overloaded for Node<T>* too, so we can cater for the rare case where we want a T
    // in more than one LinkedList (need to find a 'clean' way to retrieve the T* from the node pointer)
    template <typename T>
    struct Node
    {
        Node<T>* next;
        Node<T>* prev;
    };

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


/////     GENERAL RESOURCE HANDLER     /////
// NOTE Not too sure we want to use so much C++ nonsense,
// but I want to capture the general idea
// TODO Test!

// TODO Would be good to have some kind of type marker so their type can be enforce in function signatures, etc
template <typename T>
struct ResourceHandle
{
    u32 locator;
};

template <typename T>
struct ResourcePool
{
    Array<T> pool;
    u32 nextFreeItem;
    u32 _locatorIndexMask;

    ResourcePool( MemoryArena* arena, u32 maxResourceCount )
    {
        resourceBase = Array<T>( arena, maxResourceCount );
        _locatorIndexMask = NextPowerOf2( maxResourceCount ) - 1;
    }

    ResourceHandle Add( const T& item )
    {
        // Generate fingerprint
        u32 fingerprint = Fletcher32( &item, sizeof(T) );
        u32 index = FindNextFreeIndex();
        u32 locator = (fingerprint & ~_locatorIndexMask) | (index & _locatorIndexMask);

        // TODO Enforce this somehow in the template?
        item.locator = locator;
        pool.Insert( item, index );

        ResourceHandle result = { locator };
        return result;
    }

    // NOTE Returned pointer should never be stored or passed around!
    T* GetTransientPointer( ResourceHandle handle )
    {
        u32 index = handle.locator & _locatorIndexMask;
        ASSERT( index < pool.count );
        T* result = resourceBase + index;
        // Make sure the high bits are also the same
        // NOTE Expects a 'locator' field in the resource type
        ASSERT( handle.locator == result->locator );
        return result;
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
