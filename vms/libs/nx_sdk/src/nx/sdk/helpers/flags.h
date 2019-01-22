#pragma once

#include <type_traits>

/**@file
 * Defines operator&() and operator|() for enum classes - used for flags.
 */

template<typename Enum, std::enable_if<std::is_enum<Enum>::value>* dummy = nullptr>
Enum operator|(Enum first, Enum second)
{
    return static_cast<Enum>(
        static_cast<std::underlying_type<Enum>::type>(first)
        | static_cast<std::underlying_type<Enum>::type>(second));
}

template<typename Enum, std::enable_if<std::is_enum<Enum>::value>* dummy = nullptr>
Enum operator&(Enum first, Enum second)
{
    return static_cast<Enum>(
        static_cast<std::underlying_type<Enum>::type>(first)
        & static_cast<std::underlying_type<Enum>::type>(second));
}
