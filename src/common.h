#ifndef __DEFS_H__
#define __DEFS_H__ 

#if NON_UNITY_BUILD
#include <stdint.h>
#include <math.h>
#endif

//
// Common definitions
//

#define internal static
#define persistent static


#define HALT() (__debugbreak(), 1) //( (*(volatile int *)0x0A55 = 0) != 0 )
#if !RELEASE
#define ASSERT(expr) ((void)( !(expr) && (globalAssertHandler( #expr, __FILE__, __LINE__ ), 1) && HALT()))
#define ASSERTM(expr, msg) ((void)( !(expr) && (globalAssertHandler( msg, __FILE__, __LINE__ ), 1) && HALT()))
#else
#define ASSERT(expr) ((void)0)
#define ASSERTM(expr, msg) ((void)0)
#endif

#define ASSERT_HANDLER(name) void name( const char* msg, const char* file, int line )
typedef ASSERT_HANDLER(AssertHandlerFunc);

ASSERT_HANDLER(DefaultAssertHandler);
AssertHandlerFunc* globalAssertHandler = DefaultAssertHandler;

#if !RELEASE
#define NOT_IMPLEMENTED ASSERT(!"NotImplemented")
#else
#define NOT_IMPLEMENTED NotImplemented!!!
#endif

#define INVALID_CODE_PATH ASSERT(!"InvalidCodePath");
#define INVALID_DEFAULT_CASE default: { INVALID_CODE_PATH; } break;


#define ARRAYCOUNT(array) (sizeof(array) / sizeof((array)[0]))
#define OFFSETOF(type, member) ((sz)&(((type *)0)->member))
#define STR(s) _STR(s)
#define _STR(s) #s

#define KILOBYTES(value) ((value)*1024)
#define MEGABYTES(value) (KILOBYTES(value)*1024)
#define GIGABYTES(value) (MEGABYTES((u64)value)*1024)

#define COPY(source, dest) memcpy( &dest, &source, sizeof(dest) )
#define EQUAL(source, dest) (memcmp( &source, &dest, sizeof(source) ) == 0)
#define PCOPY(source, dest, size) memcpy( dest, source, size )
#define PSET(dest, value, size) memset( dest, value, size )
#define PZERO(dest, size) memset( dest, 0, size )


#if _MSC_VER
#define INLINE __forceinline
#else
#define INLINE inline __attribute__((always_inline))
#endif


// NOTE Only use these where storage is important (i.e: structs)
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef unsigned long long u64;

typedef float r32;
typedef double r64;

typedef size_t sz;

#define U8MAX UINT8_MAX
#define I32MAX INT32_MAX
#define I32MIN INT32_MIN
#define U32MAX UINT32_MAX
#define U64MAX UINT64_MAX
#define I64MAX INT64_MAX
#define I64MIN INT64_MIN

#define R32MAX FLT_MAX
#define R32MIN FLT_MIN
#define R32INF INFINITY
#define R32NAN NAN
#define R64INF (r64)INFINITY



/////     STRUCT ENUM    /////
// TODO Combine with the ideas in https://blog.paranoidcoding.com/2010/11/18/discriminated-unions-in-c.html to create a similar
// TAGGED_UNION for discriminated unions

/* Usage:

struct V
{
    char const* sVal;
    int iVal;
};

#define VALUES(x) \
    x(None, {"a", 100}) \
    x(Animation, {"b", 200}) \
    x(Landscape, {"c", 300}) \
    x(Audio, {"d", 400}) \
    x(Network, {"e", 500}) \
    x(Scripting, {"f", 600}) \

STRUCT_ENUM_WITH_VALUES(MemoryTag, V, VALUES)
#undef VALUES

int main()
{
    MemoryTag const& t = MemoryTag::Audio();
    V const& value = MemoryTag::Landscape().value;

    for( int i = 0; i < MemoryTag::Values::count; ++i )
    {
        MemoryTag const& t = MemoryTag::Values::items[i];
        std::cout << "sVal: " << t.value.sVal << "\tiVal: " << t.value.iVal << std::endl;
    }
}

*/

#define _ENUM_BUILDER(x) static constexpr EnumName x() \
    { static EnumName _inst = { #x, (u32)Enum::x, ValueType() }; return _inst; }
#define _ENUM_BUILDER_WITH_NAMES(x, n) static constexpr EnumName x() \
    { static EnumName _inst = { n, (u32)Enum::x, ValueType() }; return _inst; }
#define _ENUM_BUILDER_WITH_VALUES(x, v) static constexpr EnumName x() \
    { static EnumName _inst = { #x, (u32)Enum::x, v }; return _inst; }
#define _ENUM_ENTRY(x, ...) x,
#define _ENUM_NAME(x, ...) x().name,
#define _ENUM_ITEM(x, ...) x(),

#define _CREATE_ENUM(enumName, valueType, xValueList, xBuilder) \
struct enumName                                                \
{                                                              \
    char const* name;                                          \
    u32 index;                                                 \
    valueType value;                                           \
                                                               \
    using EnumName = enumName;                                 \
    using ValueType = valueType;                               \
    xValueList(xBuilder)                                       \
                                                               \
    struct Values;                                             \
                                                               \
private:                                                       \
    enum class Enum : u32                                      \
    {                                                          \
        xValueList(_ENUM_ENTRY)                                \
    };                                                         \
};                                                             \
struct enumName::Values                                        \
{                                                              \
    static constexpr char const* const names[] =               \
    {                                                          \
        xValueList(_ENUM_NAME)                                 \
    };                                                         \
    static constexpr const enumName items[] = {                \
        xValueList(_ENUM_ITEM)                                 \
    };                                                         \
    static constexpr const int count = ARRAYCOUNT(items);      \
};                                                             \

#define STRUCT_ENUM(enumName, xValueList)                           _CREATE_ENUM(enumName, u32, xValueList, _ENUM_BUILDER)
#define STRUCT_ENUM_WITH_TYPE(enumName, valueType, xValueList)      _CREATE_ENUM(enumName, valueType, xValueList, _ENUM_BUILDER)
#define STRUCT_ENUM_WITH_NAMES(enumName, valueType, xValueList)     _CREATE_ENUM(enumName, valueType, xValueList, _ENUM_BUILDER_WITH_NAMES)
#define STRUCT_ENUM_WITH_VALUES(enumName, valueType, xValueList)    _CREATE_ENUM(enumName, valueType, xValueList, _ENUM_BUILDER_WITH_VALUES)



#endif /* __DEFS_H__ */
