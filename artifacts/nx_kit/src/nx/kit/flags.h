// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Facilities to enable bit flags functionality for enums.
 *
 * Usage example:
 * <pre><code>
 *
 *     #include <nx/kit/flags.h>
 *     #include <nx/kit/debug.h>
 *
 *     enum class Color: unsigned char
 *     {
 *         black = 0,
 *
 *         red = 1 << 0,
 *         green = 1 << 1,
 *         blue = 1 << 2,
 *
 *         yellow = red | green,
 *         magenta = red | blue,
 *         cyan = green | blue,
 *
 *         white = red | green | blue,
 *     };
 *     NX_KIT_ENABLE_FLAGS(Color);
 *     
 *     // Class member enumerations are supported.
 *     struct Border
 *     {
 *         // Unscoped enumerations are supported.
 *         enum CrossDirection
 *         {
 *             NoDirection = 0,
 *             InDirection = 1 << 0,
 *             OutDirection = 1 << 1,
 *             EitherDirection = InDirection | OutDirection,
 *         };
 *     };
 *     NX_KIT_ENABLE_FLAGS(Border::CrossDirection);
 *
 *     int main()
 *     {
 *         NX_KIT_ASSERT((Color::magenta | Color::green & ~Color::red) == Color::cyan);
 *         NX_KIT_ASSERT((Border::OutDirection | Border::InDirection) == Border::EitherDirection);
 *     }
 *
 * </code></pre>
 *
 * This header can be included in any C++ project.
 */

#include <type_traits>

#if !defined(NX_KIT_API)
    #define NX_KIT_API
#endif

namespace nx {
namespace kit {

namespace flags_detail {

template <typename Enum>
constexpr typename std::underlying_type<Enum>::type toUnderlyingType(Enum value)
{
    return static_cast<typename std::underlying_type<Enum>::type>(value);
}

} // namespace flags_detail

/**
 * Enable flag operations for a given enum. Must be invoked at namespace scope in the innermost
 * namespace containing enum declaration.
 */
#define NX_KIT_ENABLE_FLAGS(ENUM) \
    static_assert(std::is_enum<ENUM>::value, "Flag type must be an enum"); \
    \
    /** Logical NOT. Returns true if and only if none of the bits are set. */ \
    constexpr bool operator!(ENUM x) \
    { \
        return ::nx::kit::flags_detail::toUnderlyingType(x) == 0; \
    } \
    \
    /** Bitwise NOT. Unused bits are inverted as well, be sure to mask them out as */ \
    /** appropriate. */ \
    constexpr ENUM operator~(ENUM x) \
    { \
        return static_cast<ENUM>(~::nx::kit::flags_detail::toUnderlyingType(x)); \
    } \
    \
    /** Bitwise AND. */ \
    constexpr ENUM operator&(ENUM x, ENUM y) \
    { \
        return static_cast<ENUM>( \
            ::nx::kit::flags_detail::toUnderlyingType(x) \
            & ::nx::kit::flags_detail::toUnderlyingType(y)); \
    } \
    \
    /** Bitwise AND compound assignment. */ \
    inline ENUM& operator&=(ENUM& x, ENUM y) \
    { \
        return x = x & y; \
    } \
    \
    /** Bitwise OR. */ \
    constexpr ENUM operator|(ENUM x, ENUM y) \
    { \
        return static_cast<ENUM>( \
            ::nx::kit::flags_detail::toUnderlyingType(x) \
            | ::nx::kit::flags_detail::toUnderlyingType(y)); \
    } \
    \
    /** Bitwise OR compound assignment. */ \
    inline ENUM& operator|=(ENUM& x, ENUM y) \
    { \
        return x = x | y; \
    } \
    \
    /** Bitwise XOR. */ \
    constexpr ENUM operator^(ENUM x, ENUM y) \
    { \
        return static_cast<ENUM>( \
            ::nx::kit::flags_detail::toUnderlyingType(x) \
            ^ ::nx::kit::flags_detail::toUnderlyingType(y)); \
    } \
    \
    /** Bitwise XOR compound assignment. */ \
    inline ENUM& operator^=(ENUM& x, ENUM y) \
    { \
        return x = x ^ y; \
    }

} // namespace kit
} // namespace nx
