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

#if NON_UNITY_BUILD
#include <ctype.h>
#include <stdarg.h>
#include "common.h"
#include "memory.h"
#include "math_types.h"
#endif

// FIXME Get rid of these!
#include <mutex>
#include <condition_variable>




/////     DYNAMIC ARRAY    /////

template <typename T>
struct Array
{
    T *data;

//private:
    i32 count;
    i32 capacity;


    Array()
    {
        data = nullptr;
        count = 0;
        capacity = 0;
    }

    // NOTE All newly allocated arrays start empty
    Array( MemoryArena* arena, i32 capacity_, MemoryParams params = DefaultMemoryParams() )
    {
        ASSERT( capacity_ > 0 );
        data = PUSH_ARRAY( arena, T, capacity_, params );
        count = 0;
        capacity = capacity_;
    }

    // NOTE All arrays initialized from a buffer start with count equal to the full capacity
    Array( T* buffer, i32 bufferCount )
    {
        ASSERT( buffer && bufferCount > 0 );
        data = buffer;
        count = bufferCount;
        capacity = bufferCount;
    }

    void Resize( i32 new_count )
    {
        ASSERT( new_count >= 0 && new_count <= capacity );
        count = new_count;
    }

    void ResizeToCapacity()
    {
        count = capacity;
    }

    T& operator[]( int i )
    {
        ASSERT( i >= 0 && i < count );
        return data[i];
    }

    const T& operator[]( int i ) const
    {
        ASSERT( i >= 0 && i < count );
        return data[i];
    }

    T& Last()
    {
        ASSERT( count > 0 );
        return data[count - 1];
    }

    const T& Last() const
    {
        ASSERT( count > 0 );
        return data[count - 1];
    }

    // TODO Not too sure about this!
    explicit operator bool() const
    {
        return data != nullptr;
    }

    T* PushEmpty( bool clear = true )
    {
        ASSERT( count < capacity );
        T* result = data + count++;
        if( clear )
            PZERO( result, sizeof(T) );

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
        PCOPY( data, result.data, count * sizeof(T) );
        result.count = count;
        return result;
    }

    void CopyTo( Array<T>* out ) const
    {
        ASSERT( out->capacity >= count );
        PCOPY( data, out->data, count * sizeof(T) );
        out->count = count;
    }

    void CopyTo( T* buffer ) const
    {
        PCOPY( data, buffer, count * sizeof(T) );
    }

    void CopyFrom( const T* buffer, int count_ )
    {
        ASSERT( count_ > 0 );
        count = count_;
        PCOPY( buffer, data, count * sizeof(T) );
    }

    bool Contains( const T& item ) const
    {
        bool result = false;
        for( int i = 0; i < count; ++i )
        {
            if( data[i] == item )
            {
                result = true;
                break;
            }
        }

        return result;
    }

    void Clear()
    {
        count = 0;
    }

    i32 Available() const
    {
        return capacity - count;
    }
};

#define ARRAY(type, count, name) type _##name[count];Array<type> name( _##name, count );


// TODO Rename this to Grid2D and try to remove the row/col semantics and just use x/y (also add operator())
template <typename T>
struct Array2
{
    i32 rows;
    i32 cols;
    T *data;


    Array2()
    {
        rows = 0;
        cols = 0;
        data = nullptr;
    }

    Array2( MemoryArena* arena, i32 rows_, i32 cols_, MemoryParams params = DefaultMemoryParams() )
    {
        ASSERT( rows_ > 0 && cols_ > 0 );
        rows = rows_;
        cols = cols_;
        data = PUSH_ARRAY( arena, T, rows * cols, params );
    }

    T& AtRowCol( int r, int c )
    {
        ASSERT( r >= 0 && r < rows );
        ASSERT( c >= 0 && c < cols );
        return data[r * cols + c];
    }

    const T& AtRowCol( int r, int c ) const
    {
        ASSERT( r >= 0 && r < rows );
        ASSERT( c >= 0 && c < cols );
        return data[r * cols + c];
    }

    // NOTE x -> cols, y -> rows
    T& At( const v2i& v )
    {
        ASSERT( v.y >= 0 && v.y < rows );
        ASSERT( v.x >= 0 && v.x < cols );
        return data[v.y * cols + v.x];
    }

    const T& At( const v2i& v ) const
    {
        ASSERT( v.y >= 0 && v.y < rows );
        ASSERT( v.x >= 0 && v.x < cols );
        return data[v.y * cols + v.x];
    }

    T* AtRow( int r )
    {
        ASSERT( r >= 0 && r < rows );
        return &data[r * cols];
    }

    const T* AtRow( int r ) const
    {
        ASSERT( r >= 0 && r < rows );
        return &data[r * cols];
    }

#if 1
    T& operator[]( int i )
    {
        ASSERT( i < Count() );
        return data[i];
    }

    const T& operator[]( int i ) const
    {
        ASSERT( i < Count() );
        return data[i];
    }
#endif

    // TODO Not too sure about this!
    explicit operator bool() const
    {
        return data != nullptr;
    }

    // Deep copy
    void CopyTo( Array2<T>* out ) const
    {
        ASSERT( rows == out->rows );
        ASSERT( cols == out->cols );
        PCOPY( data, out->data, rows * cols * sizeof(T) );
    }

    int Count() const
    {
        return rows * cols;
    }

    sz Size() const
    {
        return rows * cols * sizeof(T);
    }

    v2i XYForIndex( int i ) const
    {
        v2i result = { i % cols, i / cols };
        return result;
    }

    int IndexForRowCol( int r, int c ) const
    {
        return r * cols + c;
    }
};



/////     GRID (wrapper for multi-dimensional arrays)   /////

template <typename T>
struct Grid3D
{
    T* data;
    v3i dims;


    Grid3D()
    {
        data = nullptr;
        dims = V3iZero;
    }

    Grid3D( MemoryArena* arena, v3i dims_, MemoryParams params = DefaultMemoryParams() )
    {
        ASSERT( dims_.x > 0 );
        ASSERT( dims_.y > 0 );
        ASSERT( dims_.z > 0 );
        dims = dims_;
        data = PUSH_ARRAY( arena, T, dims.x * dims.y * dims.z, params );
    }

    INLINE T& operator()( int x, int y, int z )
    {
        ASSERT( x >= 0 && x < dims.x );
        ASSERT( y >= 0 && y < dims.y );
        ASSERT( z >= 0 && z < dims.z );
        return data[ z * dims.y * dims.x + y * dims.x + x ];
    }

    INLINE T const& operator()( int x, int y, int z ) const
    {
        ASSERT( x >= 0 && x < dims.x );
        ASSERT( y >= 0 && y < dims.y );
        ASSERT( z >= 0 && z < dims.z );
        return data[ z * dims.y * dims.x + y * dims.x + x ];
    }

#if 0
    INLINE T& operator()( v3u const& v )
    {
        ASSERT( v.x < dims.x && v.y < dims.y && v.z < dims.z );
        return data[ v.z * dims.y * dims.x + v.y * dims.x + v.x ];
    }

    INLINE T const& operator()( v3u const& v ) const
    {
        ASSERT( v.x < dims.x && v.y < dims.y && v.z < dims.z );
        return data[ v.z * dims.y * dims.x + v.y * dims.x + v.x ];
    }
#endif

    INLINE T& operator()( v3i const& v )
    {
        ASSERT( v.x >= 0 && v.x < dims.x );
        ASSERT( v.y >= 0 && v.y < dims.y );
        ASSERT( v.z >= 0 && v.z < dims.z );
        return data[ v.z * dims.y * dims.x + v.y * dims.x + v.x ];
    }

    INLINE T const& operator()( v3i const& v ) const
    {
        ASSERT( v.x >= 0 && v.x < dims.x );
        ASSERT( v.y >= 0 && v.y < dims.y );
        ASSERT( v.z >= 0 && v.z < dims.z );
        return data[ v.z * dims.y * dims.x + v.y * dims.x + v.x ];
    }
};


/////     RING BUFFER    /////

template <typename T>
struct RingBuffer
{
    Array<T> buffer;
    i32 headIndex;


    RingBuffer( MemoryArena* arena, i32 capacity_, MemoryParams params = DefaultMemoryParams() )
        : buffer( arena, capacity_, params )
    {
        headIndex = 0;
    }

    i32 Count() const
    {
        return buffer.count;
    }

    T* PushEmpty( bool clear = true )
    {
        ASSERT( headIndex < buffer.capacity );

        T* result = buffer.data + headIndex++;
        if( headIndex == buffer.capacity )
            headIndex = 0;
        if( buffer.count < buffer.capacity )
            buffer.count++;

        if( clear )
            PZERO( result, sizeof(T) );

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
        return buffer.Contains( item );
    }
};

/////     RING STACK    /////
// Circular stack that can push an infinite number of elements, by discarding elements from the bottom when needed

template <typename T>
struct RingStack
{
    Array<T> buffer;
    i32 topIndex;


    RingStack( MemoryArena* arena, i32 capacity_, MemoryParams params = DefaultMemoryParams() )
        : buffer( arena, 0, capacity_, params )
    {
        topIndex = 0;
    }

    i32 Count() const
    {
        return buffer.count;
    }

    T* Top()
    {
        T* result = nullptr;
        if( buffer.count )
        {
            int top = topIndex > 0 ? topIndex - 1 : buffer.capacity - 1;
            result = buffer.data + top;
        }
        return result;
    }

    // Return the item that is at the given distance from the top (0 equals the top item)
    const T& FromTop( int fromTop ) const
    {
        ASSERT( fromTop >= 0 && fromTop < buffer.count );
        int index = topIndex - fromTop - 1;
        if( index < 0 )
            index += buffer.capacity;

        return buffer[index];
    }

    T* PushEmpty( bool clear = true )
    {
        ASSERT( topIndex < buffer.capacity );

        T* result = buffer.data + topIndex++;
        if( topIndex == buffer.capacity )
            topIndex = 0;
        if( buffer.count < buffer.capacity )
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
            topIndex = topIndex > 0 ? topIndex - 1 : buffer.capacity - 1;
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
template <typename K, typename V, u32 (*H)( const K&, i32 )>
struct HashTable
{
    struct Slot
    {
        Slot* nextInHash;
        V value;
        K key;
        bool occupied;
    };


    Slot* table;
    i32 tableSize;
    i32 count;

    MemoryArena* arena;
    MemoryParams memoryParams;


    HashTable( MemoryArena* arena_, int size, MemoryParams params = DefaultMemoryParams() )
    {
        ASSERT( size > 0 );
        table = PUSH_ARRAY( arena_, Slot, size, params );
        tableSize = size;
        count = 0;
        arena = arena_;
        memoryParams = params;

        Clear();
    }

    // Disallow implicit copying
    HashTable( const HashTable& ) = delete;
    //HashTable& operator =( const HashTable& ) = delete;

    void Clear()
    {
        for( int i = 0; i < tableSize; ++i )
            table[i].occupied = false;
        // FIXME Add existing externally chained elements to a free list like in BucketArray
        count = 0;
    }

    int IndexFromKey( const K& key ) const
    {
        u32 hashValue = H( key, tableSize );
        int result = I32( hashValue % tableSize );
        return result;
    }

    const V* _Find( const K& key ) const
    {
        int idx = IndexFromKey( key );

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

    V* Find( const K& key )
    {
        return (V*)_Find( key );
    }
    const V* Find( const K& key ) const
    {
        return _Find( key );
    }

#if 0
    V& operator[]( const K& key )
    {
        return *Find( key );
    }
#endif

    V* InsertEmpty( const K& key )
    {
        int idx = IndexFromKey( key );

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

            slot = PUSH_STRUCT( arena, Slot, memoryParams );
            prev->nextInHash = slot;
        }

        count++;
        slot->occupied = true;
        slot->key = key;
        slot->nextInHash = nullptr;
        PZERO( &slot->value, sizeof(V) );

        return &slot->value;
    }

    bool Insert( const K& key, const V& value )
    {
        bool result = false;

        V* slotValue = InsertEmpty( key );
        if( slotValue )
        {
            *slotValue = value;
            result = true;
        }

        return result;
    }

    Array<K> Keys( MemoryArena* arena_, MemoryParams params = DefaultMemoryParams() ) const
    {
        Array<K> result( arena_, count, params );
        for( int i = 0; i < tableSize; ++i )
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

    Array<V> Values( MemoryArena* arena_, MemoryParams params = DefaultMemoryParams() ) const
    {
        Array<V> result( arena_, count, params );
        for( int i = 0; i < tableSize; ++i )
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
inline i32
Length( const char* cstring )
{
    return I32( strlen( cstring ) );
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
    i32 size;
    i32 maxSize;
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

    String( const char *cString, i32 size_, i32 maxSize_ )
    {
        ASSERT( size_ > 0 && maxSize_ > 0 );
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
        target->data = PUSH_ARRAY( arena, char, size );
        PCOPY( data, (char*)target->data, size * sizeof(char) );
    }

    const char* CString( MemoryArena* arena, MemoryParams params = DefaultMemoryParams() ) const
    {
        char* result = PUSH_ARRAY( arena, char, size + 1, params );
        PCOPY( data, result, size * sizeof(char) );

        return result;
    }

    bool IsNullOrEmpty() const
    {
        return data == nullptr || *data == '\0' || size == 0;
    }

    explicit operator bool() const
    {
        return !IsNullOrEmpty();
    }

    bool IsEqual( const char* cString, bool caseSensitive = true ) const
    {
        if( IsNullOrEmpty() )
            return false;

        bool result = false;

        if( caseSensitive )
            result = strncmp( data, cString, Sz( size ) ) == 0 && cString[size] == '\0';
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
        int remaining = size;
        while( remaining > 0 && *prospect )
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
        int lineLen = size;

        const char* atNL = data + size;
        const char* onePastNL = FindString( "\n" );

        if( onePastNL )
        {
            atNL = onePastNL;
            onePastNL++;
            // Account for stupid Windows "double" NLs
            if( *(atNL - 1) == '\r' )
                atNL--;

            lineLen = I32( onePastNL - data );
        }

        ASSERT( lineLen <= size );
        String line( data, I32( atNL - data ), maxSize );

        data = onePastNL;
        size -= lineLen;
        maxSize -= lineLen;

        return line;
    }

    int ConsumeWhitespace()
    {
        const char *start = data;
        int remaining = size;

        while( *start && IsWhitespace( *start ) && remaining > 0 )
        {
            start++;
            remaining--;
        }

        int advanced = size - remaining;

        data = start;
        size = remaining;
        maxSize -= advanced;

        return advanced;
    }

    // Consume next word trimming whatever whitespace is there at the beginning
    String ConsumeWord()
    {
        ConsumeWhitespace();

        int wordLen = 0;
        if( *data && size > 0 )
        {
            const char *end = data;
            while( *end && IsWord( *end ) )
                end++;

            wordLen = I32( end - data );
        }

        ASSERT( wordLen <= size );
        String result( data, wordLen, maxSize );

        // Consume stripped part
        data += wordLen;
        size -= wordLen;
        maxSize -= wordLen; 

        return result;
    }

    String Consume( int charCount )
    {
        const char *next = data;
        int remaining = Min( charCount, size );

        while( *next && remaining > 0 )
        {
            next++;
            remaining--;
        }

        int len = I32( next - data );
        ASSERT( len <= size );
        String result( data, len, maxSize );

        data = next;
        size -= len;
        maxSize -= len;

        return result;
    }

    // FIXME Make a copy in the stack/temp memory or something
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
            int remaining = size - 1;

            while( *next && remaining > 0 )
            {
                if( *next == *start && *(next - 1) != '\\' )
                    break;

                next++;
                remaining--;
            }

            ++start;
            result.data = start;
            result.size = I32( next - start );
            result.maxSize = maxSize;
        }

        return result;
    }

    bool ToI32( i32* output )
    {
        bool result = false;

        char* end = nullptr;
        *output = strtol( data, &end, 0 );

        if( *output == 0 )
            result = (end != nullptr);
        else if( *output == LONG_MAX || *output == LONG_MIN )
            result = (errno != ERANGE);
        else
            result = true;

        return result;
    }

    bool ToU32( u32* output )
    {
        bool result = false;

        char* end = nullptr;
        *output = strtoul( data, &end, 0 );

        if( *output == 0 )
            result = (end != nullptr);
        else if( *output == ULONG_MAX )
            result = (errno != ERANGE);
        else
            result = true;

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
        i32 capacity;

        i32 count;

        Bucket *next;
        Bucket *prev;

        Bucket( MemoryArena* arena, i32 capacity_, MemoryParams params )
        {
            ASSERT( capacity_ > 0 );
            data = PUSH_ARRAY( arena, T, capacity_, params );
            capacity = capacity_;
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
        i32 index;

        explicit operator bool() const
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
            return base && index >= 0 && index < base->count;
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
    i32 count;

    MemoryArena* arena;
    MemoryParams memoryParams;


    BucketArray( MemoryArena* arena_, i32 bucketSize, MemoryParams params = DefaultMemoryParams() )
        : first( arena_, bucketSize, params )
    {
        count = 0;
        last = &first;
        firstFree = nullptr;
        arena = arena_;
        memoryParams = params;
    }

    // Disallow implicit copying
    BucketArray( const BucketArray& ) = delete;
    //BucketArray& operator =( const BucketArray& ) = delete;

    T* PushEmpty()
    {
        if( last->count == last->capacity )
            AddEmptyBucket();

        count++;
        return &last->data[last->count++];
    }

    T* Push( const T& item )
    {
        T* slot = PushEmpty();
        *slot = item;
        return slot;
    }

    T Remove( const Idx& index )
    {
        ASSERT( count > 0 );
        ASSERT( index.IsValid() );

        // TODO Maybe add support for removing and shifting inside a bucket and make a separate RemoveSwap() method
        T result = (T&)index;

        ASSERT( last->count > 0 );
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

    void CopyTo( T* buffer, int bufferCount ) const
    {
        ASSERT( count <= bufferCount );

        const Bucket* bucket = &first;
        while( bucket )
        {
            PCOPY( bucket->data, buffer, bucket->count * sizeof(T) );
            buffer += bucket->count;
            bucket = bucket->next;
        }
    }

    void CopyTo( Array<T>* array ) const
    {
        ASSERT( count <= array->capacity );
        array->Resize( count );

        T* buffer = array->data;
        const Bucket* bucket = &first;
        while( bucket )
        {
            PCOPY( bucket->data, buffer, bucket->count * sizeof(T) );
            buffer += bucket->count;
            bucket = bucket->next;
        }
    }

    Array<T> ToArray( MemoryArena* arena_ ) const
    {
        Array<T> result( arena_, count );
        result.ResizeToCapacity();
        CopyTo( &result );

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

    T& operator[]( int i )
    {
        ASSERT( i >= 0 && i < count );

        Bucket* base = &first;
        int index = i;

        while( index >= base->count )
        {
            index -= base->count;
            base = base->next;
        }

        return base->data[index];
    }

    const T& operator[]( int i ) const
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
            newBucket = PUSH_STRUCT( arena, Bucket, memoryParams );
            *newBucket = Bucket( arena, first.capacity, memoryParams );
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
    // in more than one LinkedList
    // (need to find a 'clean' way to retrieve the T* from the node pointer, specify Node field as part of the
    // template signature somehow)
    template <typename T>
    struct Node
    {
        Node<T>* next;
        Node<T>* prev;
    };

    // Dummy value so that the list always has something in it, and we don't have to check
    // for that edge case when inserting/removing (and we never have nullptrs!)
    T sentinel;
    i32 count;

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
        ASSERT( count > 0 );
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
private:
    u32 locatorIndexMask;


public:
    ResourcePool( MemoryArena* arena, u32 maxResourceCount )
    {
        resourceBase = Array<T>( arena, maxResourceCount );
        locatorIndexMask = NextPowerOf2( maxResourceCount ) - 1;
    }

    ResourceHandle<T> Add( const T& item )
    {
        // Generate fingerprint
        u32 fingerprint = Fletcher32( &item, sizeof(T) );
        u32 index = FindNextFreeIndex();
        u32 locator = (fingerprint & ~locatorIndexMask) | (index & locatorIndexMask);

        // TODO Enforce this somehow in the template?
        item.locator = locator;
        pool.Insert( item, index );

        ResourceHandle result = { locator };
        return result;
    }

    // NOTE Returned pointer should never be stored or passed around!
    T* GetTransientPointer( ResourceHandle<T> handle )
    {
        u32 index = handle.locator & locatorIndexMask;
        ASSERT( index < pool.count );
        T* result = resourceBase + index;
        // Make sure the high bits are also the same
        // NOTE Expects a 'locator' field in the resource type
        ASSERT( handle.locator == result->locator );
        return result;
    }
};


// FIXME Do this right
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
    //dynArray.capacity = ARRAYCOUNT(aLotOfVecs);
#endif
}

#endif /* __DATA_TYPES_H__ */
