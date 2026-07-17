// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nx::views::detail {

template<typename... Ranges>
struct CartesianProductView: std::ranges::view_interface<CartesianProductView<Ranges...>>
{
    template<typename Parent, typename... ParentRanges>
    struct Iterator
    {
        using Current = std::tuple<std::ranges::iterator_t<ParentRanges>...>;
        using reference = std::tuple<std::ranges::range_reference_t<ParentRanges>...>;
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;
        using value_type = std::tuple<std::ranges::range_value_t<Ranges>...>;
        using difference_type = std::ptrdiff_t;

        Parent* parent;
        Current current;

        constexpr reference operator*() const
        {
            return std::apply(
                [](const auto&... current) -> reference { return {*current...}; },
                current);
        }

        constexpr Iterator& operator++()
        {
            const auto advanceAxis =
                [this]<std::size_t Index>()
                {
                    auto& iterator = std::get<Index>(current);
                    auto& range = std::get<Index>(parent->ranges);

                    ++iterator;
                    const bool exhausted = std::ranges::end(range) == iterator;
                    if (exhausted && 0 != Index)
                        iterator = std::ranges::begin(range);
                    return exhausted;
                };

            std::invoke(
                [&advanceAxis]<std::size_t... Index>(std::index_sequence<Index...>)
                {
                    // Reverse the indices to carry from the last axis toward the first.
                    (... && advanceAxis.template operator()<sizeof...(Ranges) - 1 - Index>());
                },
                std::index_sequence_for<Ranges...>{});
            return *this;
        }

        constexpr void operator++(int) { ++*this; }

        constexpr bool operator==(std::default_sentinel_t) const
        {
            return std::invoke(
                [this]<std::size_t... Index>(std::index_sequence<Index...>)
                {
                    return ((std::get<Index>(current)
                        == std::ranges::end(std::get<Index>(parent->ranges))) || ...);
                },
                std::index_sequence_for<Ranges...>{});
        }

    };

    constexpr auto begin()
    {
        return Iterator<CartesianProductView, Ranges...>{
            .parent = this,
            .current = std::apply(
                [](auto&... range) { return std::tuple(std::ranges::begin(range)...); },
                ranges),
        };
    }

    constexpr auto begin() const
        requires(std::ranges::range<const Ranges> && ...)
    {
        return Iterator<const CartesianProductView, const Ranges...>{
            .parent = this,
            .current = std::apply(
                [](const auto&... range) { return std::tuple(std::ranges::begin(range)...); },
                ranges),
        };
    }

    constexpr std::default_sentinel_t end() const { return {}; }

    // Each combination chooses one item from every range, so axis sizes are multiplied.
    constexpr auto size()
        requires(std::ranges::sized_range<Ranges> && ...)
    {
        return std::apply([](auto&... range) { return (std::ranges::size(range) * ...); }, ranges);
    }

    constexpr auto size() const
        requires(std::ranges::sized_range<const Ranges> && ...)
    {
        return std::apply(
            [](const auto&... range) { return (std::ranges::size(range) * ...); },
            ranges);
    }

    std::tuple<Ranges...> ranges;
};

constexpr auto cartesianProduct()
{
    return std::views::single(std::tuple{});
}

template<std::ranges::viewable_range... Ranges>
    requires(0 != sizeof...(Ranges))
constexpr CartesianProductView<std::views::all_t<Ranges>...> cartesianProduct(Ranges&&... ranges)
{
    return {.ranges = std::tuple(std::views::all(std::forward<Ranges>(ranges))...)};
}

} // namespace nx::views::detail
