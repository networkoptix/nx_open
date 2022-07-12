// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <type_traits>

namespace nx::utils::algorithm {

namespace detail {

template<typename T, std::size_t oneSize, std::size_t twoSize>
constexpr std::array<T, oneSize + twoSize> concatTwo(
    std::array<T, oneSize> one,
    std::array<T, twoSize> two)
{
    std::array<T, oneSize + twoSize> result;
    std::size_t i = 0;

    for (std::size_t j = 0; j < one.size(); ++j)
        result[i++] = std::move(one[j]);
    for (std::size_t j = 0; j < two.size(); ++j)
        result[i++] = std::move(two[j]);

    return result;
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

/**
 * Concatenate multiple containers.
 * Aims to move data as much as possible.
 */
template<typename One, typename Two, typename... Others>
constexpr auto concat(One&& one, Two&& two, Others&&... others)
{
    auto v = detail::concatTwo(std::forward<One>(one), std::forward<Two>(two));
    if constexpr (sizeof...(others) > 0)
        return concat(std::move(v), std::forward<Others>(others)...);
    return v;
}

} // namespace nx::utils::algorithm
