// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/preprocessor/for_each.h>

#include "utils.h"

namespace nx::reflect {

namespace enumeration {

template<typename Enum>
struct Item
{
    Enum value;
    nx::reflect::detail::string_view name;
};

namespace detail {

template<typename Enum>
struct ItemValue
{
    const Enum value;

    constexpr ItemValue(Enum value): value(value) {}

    // These operators let us swallow an assignment:
    // `(ItemValue) x = 1` => `ItemValue(x) = 1` => `ItemValue(x)` => `x`
    template<typename T>
    constexpr Enum operator=(const T&) const { return value; }
    constexpr Enum operator=(Enum) { return value; }
    constexpr operator Enum() const { return value; }
};

constexpr nx::reflect::detail::string_view itemName(const char* str)
{
    int end = 0;
    for (auto c = str[end];
        c && c != ' ' && c != '\n' && c != '=' && c != '\t' && c != '\r';
        c = str[++end])
    {
    }

    nx::reflect::detail::string_view result(str, end);

    // Remove trailing '_' for names like 'default_', 'private_' and so on.
    if (!result.empty() && result[result.length() - 1] == '_')
        result = result.substr(0, result.length() - 1);
    return result;
}

struct DummyVisitor
{
    template<typename... Items>
    void operator()(Items&&...) const {}
};
constexpr DummyVisitor dummyVisitor;

// The only purpose of this is to get rid of leading comma after a macro expansion.
template<typename Visitor, typename... Items>
constexpr auto apply(Visitor&& visitor, Items... items)
{
    return visitor(items...);
}

} // namespace detail

template<typename Enum, typename Visitor>
constexpr auto visitAllItems(Visitor&& visitor)
{
    return nxReflectVisitAllEnumItems((Enum*) nullptr,
        nx::reflect::detail::forward<Visitor>(visitor));
}

// Helpers for easily accessing all enum values without having to setup your own visitor
template<typename... Items>
struct EnumCounter
{
    static constexpr int count = sizeof...(Items) - 1;
};

template<typename Enum, int N>
struct EnumArray
{
    Enum values[N];

    static constexpr int size() { return N; }

    // Iterators for range based for loops
    constexpr Enum* begin() { return values; }
    constexpr const Enum* begin() const { return values; }

    constexpr Enum* end() { return values + N; }
    constexpr const Enum* end() const { return values + N; }

    constexpr void add(int index, Enum value)
    {
        values[index] = value;
    }
};

template<typename Enum>
constexpr auto allEnumValues()
{
    return visitAllItems<Enum>(
        []<typename... Items>(Items&&... items)
        {
            constexpr int enumSize = EnumCounter<Enum, Items...>::count;
            EnumArray<Enum, enumSize> enumArray;

            int index = 0;
            (enumArray.add(index++, items.value), ...);

            return enumArray;
        });
}

} // namespace enumeration

/**
 * Checks whether T is an instrumented enum:
 * `if constexpr (nx::reflect::IsInstrumentedEnum<Foo>::value) ...`
 */
template<typename T, typename = void>
struct IsInstrumentedEnum
{
    static constexpr bool value = false;
};

template<typename T>
struct IsInstrumentedEnum<
    T,
    detail::void_t<decltype(nxReflectVisitAllEnumItems(
        (T*) nullptr, nx::reflect::enumeration::detail::dummyVisitor))>>
{
    static constexpr bool value = true;
};

template<typename... Args>
inline constexpr bool IsInstrumentedEnumV = IsInstrumentedEnum<Args...>::value;

} // namespace nx::reflect

//-------------------------------------------------------------------------------------------------
// Private macros.

#define NX_REFLECTION_DETAIL_INSTRUMENT_ENUM(PREFIX, ENUM, ...) \
    template<typename Visitor> \
    PREFIX constexpr auto nxReflectVisitAllEnumItems(ENUM*, Visitor&& visitor) \
    { \
        return nx::reflect::enumeration::detail::apply(visitor \
            NX_PP_FOR_EACH(NX_REFLECTION_DETAIL_INSTRUMENT_ENUM_ITEM, ENUM, __VA_ARGS__)); \
    }

#define NX_REFLECTION_DETAIL_INSTRUMENT_ENUM_ITEM(ENUM, ITEM) \
    , nx::reflect::enumeration::Item<ENUM>{ \
        (nx::reflect::enumeration::detail::ItemValue<ENUM>) ENUM::ITEM, \
        nx::reflect::enumeration::detail::itemName(#ITEM)}

//-------------------------------------------------------------------------------------------------

/**
 * Instruments the given ENUM promoting its items as value/name pairs.
 *
 * Example:
 * enum class MyEnum { one, two, three };
 * NX_REFLECTION_INSTRUMENT_ENUM(MyEnum, one, two, three)
 *
 * Note: This macro is created to be used from NX_REFLECTION_ENUM(_CLASS) macros, so it
 * supports manual enum value specification (x = 42).
 */
#define NX_REFLECTION_INSTRUMENT_ENUM(ENUM, ...) \
    NX_REFLECTION_DETAIL_INSTRUMENT_ENUM(/*no prefix*/, ENUM, __VA_ARGS__)

/**
 * Defines and instruments enum class.
 *
 * This macro produces an enum with the given items in __VA_ARGS__.
 *
 * Example:
 * NX_REFLECTION_ENUM_CLASS(MyEnum, one = 0, two, three)
 * This will result in:
 * enum class MyEnum { one = 0, two, three };
 * NX_REFLECTION_INSTRUMENT_ENUM(MyEnum, one = 0, two, three)
 */
#define NX_REFLECTION_ENUM_CLASS(ENUM, ...) \
    enum class ENUM { __VA_ARGS__ }; \
    NX_REFLECTION_INSTRUMENT_ENUM(ENUM, __VA_ARGS__)

/**
 * Defines and instruments enum.
 *
 * See NX_REFLECTION_ENUM_CLASS
 */
#define NX_REFLECTION_ENUM(ENUM, ...) \
    enum ENUM { __VA_ARGS__ }; \
    NX_REFLECTION_INSTRUMENT_ENUM(ENUM, __VA_ARGS__)

/** The same as NX_REFLECTION_ENUM_CLASS, but for usages inside a class scope. */
#define NX_REFLECTION_ENUM_CLASS_IN_CLASS(ENUM, ...) \
    enum class ENUM { __VA_ARGS__ }; \
    NX_REFLECTION_DETAIL_INSTRUMENT_ENUM(friend, ENUM, __VA_ARGS__)

/** The same as NX_REFLECTION_ENUM, but for usages inside a class scope. */
#define NX_REFLECTION_ENUM_IN_CLASS(ENUM, ...) \
    enum ENUM { __VA_ARGS__ }; \
    NX_REFLECTION_DETAIL_INSTRUMENT_ENUM(friend, ENUM, __VA_ARGS__)
