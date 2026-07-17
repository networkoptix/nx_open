// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include <nx/ranges/actions/append.h>

namespace nx::ranges::detail {

template<template<typename...> typename To, typename ValueType>
struct ResolveType
{
    using type = To<ValueType>;
};

template<template<typename...> typename To, typename ValueType>
    requires (!requires { typename To<ValueType>; })
        && requires(ValueType value) { value.first; value.second; }
struct ResolveType<To, ValueType>
{
    using type =
        To<std::decay_t<decltype(std::declval<ValueType>().first)>,
           std::decay_t<decltype(std::declval<ValueType>().second)>>;
};

// Container constructors take two iterators of the same type, while a range may end with a
// distinct sentinel. Adapt it when possible; otherwise retain the range for the append fallback.
template<std::ranges::viewable_range Range>
    requires std::ranges::input_range<Range>
constexpr auto commonViewIfPossible(Range&& range)
{
    if constexpr (requires { std::forward<Range>(range) | std::views::common; })
        return std::forward<Range>(range) | std::views::common;
    else
        return std::views::all(std::forward<Range>(range));
}

template<template<typename...> typename To, typename Range>
concept DeducibleFromMoveIterators =
    std::ranges::common_range<Range>
    && requires(Range& range)
    {
        To(
            std::make_move_iterator(std::ranges::begin(range)),
            std::make_move_iterator(std::ranges::end(range)));
    };

template<typename Container, typename Range>
concept ConstructibleFromIterators =
    std::ranges::common_range<Range>
    && requires(Range& range)
    {
        Container(std::ranges::begin(range), std::ranges::end(range));
    };

template<typename Container, typename Range>
concept ConstructibleFromMoveIterators =
    std::ranges::common_range<Range>
    && requires(Range& range)
    {
        Container(
            std::make_move_iterator(std::ranges::begin(range)),
            std::make_move_iterator(std::ranges::end(range)));
    };

template<typename Container>
concept Reservable =
    requires(Container& container, std::size_t size) { container.reserve(size); };

template<template<typename...> typename To>
struct ToTemplateClosure
{
    template<std::ranges::viewable_range R>
        requires std::ranges::input_range<R>
    constexpr auto operator()(R&& input) const
    {
        // Iterator-pair constructors require a common range; select the path by the adapted type.
        auto range = commonViewIfPossible(std::forward<R>(input));
        using Range = decltype(range);

        if constexpr (DeducibleFromMoveIterators<To, Range>)
        {
            // This works for `to<std::map>()` and sometimes for `to<std::vector>()`.
            return To(std::make_move_iterator(range.begin()),
                std::make_move_iterator(range.end()));
        }
        else
        {
            // Map-like containers deduce their key and value types from the range's pair-like value.
            typename ResolveType<To, std::ranges::range_value_t<Range>>::type result;

            if constexpr (std::ranges::sized_range<Range> && Reservable<decltype(result)>)
                result.reserve(std::ranges::size(range));

            for (auto&& value: range)
                nx::appendOne(&result, std::forward<decltype(value)>(value));

            return result;
        }
    }

    template<std::ranges::viewable_range Range>
        requires std::ranges::input_range<Range>
    friend constexpr auto operator|(Range&& range, ToTemplateClosure self)
    {
        return self(std::forward<Range>(range));
    }
};

template<typename Container>
struct ToClosure
{
    template<std::ranges::viewable_range R>
        requires std::ranges::input_range<R>
    constexpr auto operator()(R&& input) const
    {
        // Iterator-pair constructors require a common range; select the path by the adapted type.
        auto range = commonViewIfPossible(std::forward<R>(input));
        using Range = decltype(range);

        // Fixes std::string from range of char.
        if constexpr (ConstructibleFromIterators<Container, Range>)
        {
            return Container(range.begin(), range.end());
        }
        else if constexpr (ConstructibleFromMoveIterators<Container, Range>)
        {
            // Moves for non-char types.
            return Container(
                std::make_move_iterator(range.begin()),
                std::make_move_iterator(range.end()));
        }
        else
        {
            Container result{};
            if constexpr (std::ranges::sized_range<Range> && Reservable<Container>)
                result.reserve(std::ranges::size(range));

            for (auto&& value: range)
                nx::appendOne(&result, std::forward<decltype(value)>(value));

            return result;
        }
    }

    template<std::ranges::viewable_range Range>
        requires std::ranges::input_range<Range>
    friend constexpr auto operator|(Range&& range, ToClosure self)
    {
        return self(std::forward<Range>(range));
    }
};

// Limited adapter.
template<template<typename...> typename To>
constexpr ToTemplateClosure<To> to()
{
    return {};
}

// Overload for concrete container types (e.g., std::string, std::vector<std::string>).
template<typename Container>
constexpr ToClosure<Container> to()
{
    return {};
}

} // namespace nx::ranges::detail
