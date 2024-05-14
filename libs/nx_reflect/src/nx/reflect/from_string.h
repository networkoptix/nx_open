// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <type_traits>

#include "enum_string_conversion.h"
#include "type_utils.h"

namespace nx::reflect {

namespace detail {

template<typename T, typename = std::void_t<>>
struct IsQByteArrayAlike: std::false_type {};

template<typename T>
struct IsQByteArrayAlike<
    T,
    std::void_t<decltype(T::fromBase64(std::declval<decltype(
        T::fromRawData((const char*) nullptr, (std::size_t) 0))>()))>
>: std::true_type {};

template<typename... Args>
inline constexpr bool IsQByteArrayAlikeV = IsQByteArrayAlike<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = std::void_t<>>
struct HasFromBase64: std::false_type {};

template<typename T>
struct HasFromBase64<
    T,
    std::void_t<decltype(T::fromBase64(std::declval<std::string_view>()))>
>: std::true_type {};

template<typename... Args>
inline constexpr bool HasFromBase64V = HasFromBase64<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = std::void_t<>>
struct HasFromStdString: std::false_type {};

template<typename T>
struct HasFromStdString<
    T,
    std::void_t<decltype(T::fromStdString(std::declval<std::string>()))>
>: std::true_type {};

template<typename... Args>
inline constexpr bool HasFromStdStringV = HasFromStdString<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = std::void_t<>>
struct HasFromString: std::false_type {};

template<typename T>
struct HasFromString<
    T,
    std::void_t<decltype(T::fromString(std::declval<std::string>()))>
>: std::true_type {};

template<typename... Args>
inline constexpr bool HasFromStringV = HasFromString<Args...>::value;

//-------------------------------------------------------------------------------------------------

template<
    typename String, typename T,
    typename = std::enable_if_t<std::is_same_v<String, std::string_view>>
>
bool fromString(const String& str, T* result)
{
    // No proper fromString overload is available for type T.
    if constexpr (detail::IsQByteArrayAlikeV<T>)
    {
        // Expecting QByteArray-like type here.
        int size = str.size();
        while (size > 0 && str[size - 1] == '=')
            --size;
        constexpr int kBase64Limit = 128 * 1024 * 1024;
        result->reserve((size / 4) * 3 + ((size % 4) + 1) / 2 + 1);
        for (int i = 0; i < size; i += kBase64Limit)
        {
            result->append(
                T::fromBase64(T::fromRawData(str.data() + i, std::min(kBase64Limit, size - i))));
        }
        return true;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        const auto fromCharsResult = std::from_chars(str.data(), str.data() + str.size(), *result);
        return fromCharsResult.ptr != str.data();
    }
    else if constexpr (detail::HasFromBase64V<T>)
    {
        // Expecting QByteArray-like type here.
        *result = T::fromBase64(str);
        return true;
    }
    else if constexpr (detail::HasFromStdStringV<T>)
    {
        *result = T::fromStdString(std::string(str));
        return true;
    }
    else if constexpr (detail::HasFromStringV<T>)
    {
        // TODO: #akolesnikov Invoke T::fromString(const std::string_view&) if present.
        *result = T::fromString(std::string(str));
        return true;
    }
    else if constexpr (nx::reflect::IsInstrumentedEnumV<T>)
    {
        return nx::reflect::enumeration::fromString(str, result);
    }
    else if constexpr (nx::reflect::IsStdChronoDurationV<T>)
    {
        *result = T(std::stoll(std::string(str)));
        return true;
    }
    else
    {
        // There is a fromString that accepts std::string?
        return fromString(std::string(str), result);
    }
}

// template<typename T>
// concept Stringizable = HasFromBase64V<T> ||  HasFromStdStringV<T> || HasFromStringV<T>;

/**
 * Converts str to T using the following methods (if present):
 * bool fromString(const std::string_view&, T*);
 * T T::fromBase64(const Stringizable&);
 * T T::fromString(const std::string&);
 * T T::fromStdString(const std::string&);
 * bool fromString(const std::string&, T*);
 */
template<typename T>
bool fromStringSfinae(const std::string_view& str, T* result)
{
    return fromString(str, result);
}

inline bool fromStringSfinae(const std::string_view& str, std::string* result)
{
    *result = std::string(str);
    return true;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

template<typename T>
T fromString(const std::string_view& str, bool* ok = nullptr)
{
    T val;
    const auto res = detail::fromStringSfinae(str, &val);
    if (ok)
        *ok = res;
    return val;
}

template<typename T>
T fromString(const std::string_view& str, T defaultValue, bool* ok = nullptr)
{
    T val;
    const auto res = detail::fromStringSfinae(str, &val);
    if (ok)
        *ok = res;
    return res ? val : defaultValue;
}

/**
 * Converts str to T using the following methods (if present):
 * bool fromString(const StringType&, T*);
 * T T::fromString(const StringType&);
 * T T::fromStdString(const StringType&);
 * T T::fromBase64(const StringType&);
 * `StringType` can be either `std::string` or `std::string_view`.
 * @return true if the conversion was successful. Note that not every underlying conversion method
 * provides result.
 */
template<typename T>
bool fromString(const std::string_view& str, T* result)
{
    return detail::fromStringSfinae(str, result);
}

} // namespace nx::reflect
