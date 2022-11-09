// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * This file contains partial implementation of <charconv> header.
 * Needed because of the lack of support on macosx.
 * Limitations:
 * - to from_chars / to_chars only are implemented
 * - only 10 and 16 bases are supported for integers
 * - only std::chars_format::general format is supported for floats
 */

#if defined(__cpp_lib_to_chars)

#include <charconv>

namespace nx::utils::charconv {

using std::chars_format;

using std::from_chars_result;
using std::from_chars;

using std::chars_format;

using std::to_chars_result;
using std::to_chars;

} // namespace nx::utils::charconv

#else

// NOTE: The partial implementation available on Macosx has some weird problems.
#if !__has_include(<charconv>) || __APPLE__
#   define NX_CHARCONV_INTEGER
#   define NX_CHARCONV_FROM_FLOAT
#   define NX_CHARCONV_TO_FLOAT
#elif defined(__GNUG__)
    // libstdc++ provides partial <charconv> implementation as of gcc 8.1.
#   define NX_CHARCONV_FROM_FLOAT
#   define NX_CHARCONV_TO_FLOAT
#endif

// NOTE: msvc declares to_chars(..., float, ...) as "=delete" making it impossible
// to implement them locally.
// If needed, this <charconv> implementation may be moved to a new namespace.
// #define NX_CHARCONV_TO_FLOAT

#include <array>
#include <cstdio>
#include <cstdlib>
#include <system_error>

#if !defined(NX_CHARCONV_INTEGER)
#include <charconv>
#endif

#include "../log/assert.h"

namespace nx::utils::charconv {

#if defined(NX_CHARCONV_INTEGER)

struct from_chars_result {
    const char* ptr;
    std::errc ec;
};

struct to_chars_result {
    char* ptr;
    std::errc ec;
};

#else

using std::from_chars_result;
using std::to_chars_result;

#endif // #if defined(NX_CHARCONV_INTEGER)

#if defined(NX_CHARCONV_FROM_FLOAT) || defined(NX_CHARCONV_TO_FLOAT)

enum class chars_format {
    scientific = 1,
    fixed = 2,
    hex = 4,
    general = fixed | scientific
};

#else

using std::chars_format;

#endif // #if defined(NX_CHARCONV_FROM_FLOAT) || defined(NX_CHARCONV_TO_FLOAT)

//-------------------------------------------------------------------------------------------------

namespace detail {

template<typename T, typename Func, typename... Args>
// requires std::is_arithmetic_v<T>
from_chars_result from_chars(
    const char* first, const char* last,
    T& value,
    Func func,
    Args... args)
{
    const char* realFirst = first;
    while (isspace(*realFirst) && realFirst < last)
        ++realFirst;

    //NOTE: strtoll and similar do not support string end.
    char buf[32];
    if ((std::size_t)(last - realFirst) > sizeof(buf) - 1)
        return {first, std::errc::invalid_argument};

    memcpy(buf, realFirst, last - realFirst);
    buf[last - realFirst] = '\0';

    char* strEnd = nullptr;
    value = func(buf, &strEnd, args...);
    if (strEnd == buf)
        return {first, std::errc::invalid_argument};

    return {realFirst + (strEnd - buf), std::errc()};
}

template<typename T, typename Func, typename X = std::enable_if_t<std::is_integral_v<T>>>
// requires std::is_integral_v<T>
from_chars_result int_from_chars(
    const char* first, const char* last,
    T& value,
    Func func,
    int base)
{
    if ((base != 0 && base < 8) || (last <= first))
        return {first, std::errc::invalid_argument};

    return from_chars(first, last, value, func, base);
}

//-------------------------------------------------------------------------------------------------

template<typename T>
to_chars_result to_chars(char* first, char* last, T value, const char* format)
{
    static constexpr size_t kBufferSize = 32;
    char buf[kBufferSize];

    auto len = std::snprintf(buf, kBufferSize, format, value);

    if (len > last - first)
        return {last, std::errc::value_too_large};

    memcpy(first, buf, len);
    return {first + len, std::errc()};
}

template<typename T>
std::array<char, 5> prepareIntFormatSpecifier(int base)
{
    std::array<char, 5> buf;
    char* fmt = &buf.front();
    *(fmt++) = '%';
    if constexpr (std::is_same_v<std::make_signed_t<T>, long>)
    {
        *(fmt++) = 'l';
    }
    else if constexpr (std::is_same_v<std::make_signed_t<T>, long long>)
    {
        *(fmt++) = 'l';
        *(fmt++) = 'l';
    }

    if constexpr (std::is_signed_v<T>)
        *(fmt++) = 'd';
    else
        *(fmt++) = 'u';

    if (base == 16)
        *(fmt - 1) = 'x';

    *(fmt++) = '\0';
    return buf;
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
// requires std::is_integral_v<T>
to_chars_result int_to_chars(char* first, char* last, T value, int base)
{
    NX_ASSERT(base == 10 || base == 16);

    if (base != 10 && base != 16)
        return {first, std::errc::invalid_argument};

    auto format = prepareIntFormatSpecifier<T>(base);
    return to_chars(first, last, value, format.data());
}

template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
// requires std::is_floating_point_v<T>
to_chars_result float_to_chars(
    char* first, char* last,
    T value,
    chars_format fmt, int precision)
{
    NX_ASSERT(fmt == chars_format::general);

    if (fmt != chars_format::general)
        return {first, std::errc::invalid_argument};

    static constexpr size_t kFormatSize = 16;
    char format[kFormatSize];
    snprintf(format, kFormatSize, "%%.%dg", precision);
    return to_chars(first, last, value, format);
}

} // namespace detail

//-------------------------------------------------------------------------------------------------
// integer types support.

#if defined(NX_CHARCONV_INTEGER)

inline from_chars_result from_chars(
    const char* first, const char* last,
    unsigned long long& value, int base = 10)
{
    return nx::utils::charconv::detail::int_from_chars(first, last, value, &strtoull, base);
}

inline from_chars_result from_chars(
    const char* first, const char* last,
    long long& value, int base = 10)
{
    return nx::utils::charconv::detail::int_from_chars(first, last, value, &strtoll, base);
}

inline from_chars_result from_chars(
    const char* first, const char* last,
    long& value, int base = 10)
{
    return nx::utils::charconv::detail::int_from_chars(first, last, value, &strtol, base);
}

inline from_chars_result from_chars(
    const char* first, const char* last,
    unsigned long& value, int base = 10)
{
    return nx::utils::charconv::detail::int_from_chars(first, last, value, &strtoul, base);
}

inline from_chars_result from_chars(
    const char* first, const char* last,
    int& value, int base = 10)
{
    long tmpValue = 0;
    const auto result = nx::utils::charconv::detail::int_from_chars(first, last, tmpValue, &strtol, base);
    value = (int) tmpValue;
    return result;
}

inline from_chars_result from_chars(
    const char* first, const char* last,
    unsigned int& value, int base = 10)
{
    unsigned long tmpValue = 0;
    const auto result = nx::utils::charconv::detail::int_from_chars(first, last, tmpValue, &strtol, base);
    value = (unsigned int) tmpValue;
    return result;
}

//-------------------------------------------------------------------------------------------------

inline to_chars_result to_chars(char* first, char* last, long long value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned long long value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, long value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned long value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, int value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

inline to_chars_result to_chars(char* first, char* last, unsigned int value, int base = 10)
{
    return nx::utils::charconv::detail::int_to_chars(first, last, value, base);
}

#else

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
// requires std::is_integral_v<T>
inline from_chars_result from_chars(
    const char* first, const char* last,
    T& value, int base = 10)
{
    return std::from_chars(first, last, value, base);
}

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
// requires std::is_integral_v<T>
inline to_chars_result to_chars(char* first, char* last, T value, int base = 10)
{
    return std::to_chars(first, last, value, base);
}

#endif // #if defined(NX_CHARCONV_INTEGER)

#if defined(NX_CHARCONV_FROM_FLOAT)

//-------------------------------------------------------------------------------------------------
// float support.

inline from_chars_result from_chars(
    const char* first, const char* last, float& value,
    chars_format /*fmt*/ = chars_format::general)
{
    return nx::utils::charconv::detail::from_chars(first, last, value, &strtof);
}

inline from_chars_result from_chars(
    const char* first, const char* last, double& value,
    chars_format /*fmt*/ = chars_format::general)
{
    return nx::utils::charconv::detail::from_chars(first, last, value, &strtod);
}

inline from_chars_result from_chars(
    const char* first, const char* last, long double& value,
    chars_format /*fmt*/ = chars_format::general)
{
    return nx::utils::charconv::detail::from_chars(first, last, value, &strtold);
}

#else

template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
// requires std::is_floating_point_v<T>
inline from_chars_result from_chars(
    const char* first, const char* last,
    T& value,
    chars_format fmt = chars_format::general)
{
    return std::from_chars(first, last, value, fmt);
}

#endif // #if defined(NX_CHARCONV_FROM_FLOAT)

//-------------------------------------------------------------------------------------------------

#if defined(NX_CHARCONV_TO_FLOAT)

inline to_chars_result to_chars(
    char* first, char* last,
    float value,
    chars_format fmt, int precision)
{
    return nx::utils::charconv::detail::float_to_chars(first, last, value, fmt, precision);
}

inline to_chars_result to_chars(
    char* first, char* last,
    double value,
    chars_format fmt, int precision)
{
    return nx::utils::charconv::detail::float_to_chars(first, last, value, fmt, precision);
}

inline to_chars_result to_chars(
    char* first, char* last,
    long double value,
    chars_format fmt, int precision)
{
    return nx::utils::charconv::detail::float_to_chars(first, last, value, fmt, precision);
}

#else

template<typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
// requires std::is_floating_point_v<T>
inline to_chars_result to_chars(
    char* first, char* last,
    T value,
    chars_format fmt, int precision)
{
    return std::to_chars(first, last, value, fmt, precision);
}

#endif // defined(NX_CHARCONV_TO_FLOAT)

} // namespace nx::utils::charconv

#endif
