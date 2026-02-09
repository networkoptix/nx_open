// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <concepts>
#include <ranges>

namespace nx::ranges {

template<typename Range, typename Element>
concept RangeOf =
    std::ranges::range<Range> && std::same_as<std::ranges::range_value_t<Range>, Element>;

template<typename Range, typename Element>
concept ViewOf =
    std::ranges::view<Range>
    && std::same_as<std::ranges::range_value_t<Range>, Element>;

template<typename R>
concept PairLikeRange = std::ranges::range<R>
    && requires(R& r) {
    { std::get<0>(*std::ranges::begin(r)) };
    { std::get<1>(*std::ranges::begin(r)) };
};

template<typename R>
concept KeyValueRange = std::ranges::input_range<R> && requires {
    typename std::ranges::range_value_t<R>::first_type;
    typename std::ranges::range_value_t<R>::second_type;
};

template<typename K, typename R>
concept RangeKeyLike = KeyValueRange<R>
    && std::equality_comparable_with<K, typename std::ranges::range_value_t<R>::first_type>;

template<typename T, typename R>
concept RangeValueLike = KeyValueRange<R>
    && std::convertible_to<typename std::ranges::range_value_t<R>::second_type, T>;

template<typename C, typename... Args>
concept CanEmplaceBack =
    std::ranges::range<C> && std::constructible_from<std::ranges::range_value_t<C>, Args...>
    && requires(C& c, Args&&... args) { c.emplace_back(std::forward<Args>(args)...); };

template<typename C, typename... Args>
concept CanEmplace =
    std::ranges::range<C> && std::constructible_from<std::ranges::range_value_t<C>, Args...>
    && requires(C& c, Args&&... args) { c.emplace(std::forward<Args>(args)...); };

template<typename C, typename T>
concept CanPushBack =
    std::ranges::range<C> && std::constructible_from<std::ranges::range_value_t<C>, T>
    && requires(C& c, T&& v) { c.push_back(std::forward<T>(v)); };

template<typename C, typename T>
concept CanInsert =
    std::ranges::range<C> && std::constructible_from<std::ranges::range_value_t<C>, T>
    && requires(C& c, T&& v) { c.insert(std::forward<T>(v)); };

template<typename C, typename T>
concept CanAppendOne =
    CanEmplaceBack<C, T> || CanPushBack<C, T> || CanEmplace<C, T> || CanInsert<C, T>;

}
