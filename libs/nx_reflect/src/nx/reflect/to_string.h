// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <type_traits>

#include "enum_string_conversion.h"

namespace nx::reflect {

namespace detail {

template <typename T>
class HasToBase64
{
    typedef char one;
    struct two { char x[2]; };

    template <typename C> static one test(std::decay_t<decltype(std::declval<C>().toBase64())>*);
    template <typename C> static two test(...);

public:
    static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
};

template<typename... Args>
inline constexpr bool HasToBase64V = HasToBase64<Args...>::value;

//-------------------------------------------------------------------------------------------------

template <typename T>
class HasToStdString
{
    typedef char one;
    struct two { char x[2]; };

    template <typename C> static one test(std::decay_t<decltype(std::declval<C>().toStdString())>*);
    template <typename C> static two test(...);

public:
    static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
};

template<typename... Args>
inline constexpr bool HasToStdStringV = HasToStdString<Args...>::value;

//-------------------------------------------------------------------------------------------------

template <typename T>
class HasToString
{
    typedef char one;
    struct two { char x[2]; };

    template <typename C> static one test(std::decay_t<decltype(std::declval<C>().toString())>*);
    template <typename C> static two test(...);

public:
    static constexpr bool value = sizeof(test<T>(0)) == sizeof(char);
};

template<typename... Args>
inline constexpr bool HasToStringV = HasToString<Args...>::value;

//-------------------------------------------------------------------------------------------------

enum Flags
{
    disableToBase64 = 1,
};

template<int flags = 0, typename T>
std::string toStringSfinae(T&& value)
{
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        return std::forward<T>(value);
    else if constexpr (detail::HasToBase64V<std::decay_t<T>> && (flags & Flags::disableToBase64) == 0)
        // NOTE: Making sure there is no infinite recursion
        // if T::toBase64() returns T (like QByteArray),
        return toStringSfinae<Flags::disableToBase64>(value.toBase64());
    else if constexpr (detail::HasToStdStringV<std::decay_t<T>>)
        return value.toStdString();
    else if constexpr (detail::HasToStringV<std::decay_t<T>>)
        return toStringSfinae(value.toString());
    else if constexpr (nx::reflect::IsInstrumentedEnumV<std::decay_t<T>>)
        return nx::reflect::enumeration::toString(value);
    else
        return toStringSfinae(toString(value));
}

inline const std::string& toStringSfinae(const std::string& value)
{
    return value;
}

inline std::string toStringSfinae(const char* value)
{
    return std::string(value);
}

inline std::string toStringSfinae(const std::string_view& value)
{
    return std::string(value);
}

template<typename T>
void toString(const T& value, std::string* str)
{
    auto tmpStr = nx::reflect::detail::toStringSfinae(value);
    if constexpr (std::is_same_v<std::decay_t<decltype(tmpStr)>, std::string>)
        *str = tmpStr;
    else
        toString(tmpStr, str);
}

} // namespace detail

/**
 * Converts a value to a string by
 * - invoking T::toString() if it exists,
 * - invoking T::toStdString() if it exists,
 * - invoking toString(T::toBase64()) if it exists,
 * - converting enum to string if T is an instrumented enum type
 */
template<typename T>
std::string toString(const T& value)
{
    std::string str;
    nx::reflect::detail::toString(value, &str);
    return str;
}

} // namespace nx::reflect
