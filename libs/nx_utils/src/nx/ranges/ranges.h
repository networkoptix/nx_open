// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/concepts/ranges.h>
#include <nx/ranges/actions.h>
#include <nx/ranges/closure.h>
#include <nx/ranges/ranges.h>
#include <nx/ranges/views.h>

// -- Operations with Eager evaluation ----------------------------------------------------------
namespace nx::ranges {

template<KeyValueRange R>
constexpr std::ranges::range_value_t<R>::second_type valueOr(
    const R& keyValueRange,
    const RangeKeyLike<R> auto& key,
    const RangeValueLike<R> auto& orValue)
{
    const auto it = std::ranges::find_if(
        keyValueRange,
        [&key](const auto& kv)
        {
            return kv.first == key;
        });

    if (std::ranges::end(keyValueRange) != it)
        return it->second;
    return orValue;
}

constexpr Closure trim =
    [](std::string_view v) -> std::string_view
    {
        constexpr auto safeIsSpace = [](unsigned char c) { return std::isspace(c); };
        const auto first = std::ranges::find_if_not(v, safeIsSpace);
        const auto last = std::ranges::find_if_not(v.rbegin(), v.rend(), safeIsSpace).base();

        return (first < last)
            ? std::string_view(first, last)
            : std::string_view{}; //< All whitespaces result in empty view.
    };

/**
 * Takes the first N elements of a viewable range as std::array<T, N>. Range must have at least N
 * elements.
 */
template<size_t N>
constexpr Closure to_array =
    []<std::ranges::viewable_range R, typename T = std::ranges::range_value_t<R>>(R&& range)
        -> std::array<T, N>
    {
        auto it = std::ranges::begin(range);
        return std::invoke(
            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                return std::to_array((void(I), std::move(*it++))...);
            },
            std::make_index_sequence<N>{});
    };

/**
 * Takes the first two elements of a viewable range as std::pair<V,V>. Range must have at least 2
 * elements.
 */
constexpr Closure to_pair =
    []<std::ranges::viewable_range R, typename V = std::ranges::range_value_t<R>>(
        R&& range) -> std::pair<V, V>
    {
        auto it = std::ranges::begin(range);
        return std::invoke(
            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                return std::pair{(void(I), std::move(*it++))...};
            },
            std::make_index_sequence<2>{});
    };

/**
 * Distributes product (pair) over sum (variant). `X * (A + B + C) -> X * A + X * B + X * C`
 */
constexpr Closure distribute_product_over_sum =
    []<typename X, typename... A>
        (std::pair<X, std::variant<A...>> p) -> std::variant<std::pair<X, A>...>
    {
        return std::visit(
            [&]<typename U>(U&& x) -> std::variant<std::pair<X, A>...>
            {
                return std::pair{std::move(p.first), std::forward<U>(x)};
            },
            p.second);
    };

} // namespace nx::ranges
