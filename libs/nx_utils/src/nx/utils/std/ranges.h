// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cctype>
#include <functional>
#include <ranges>

#include <nx/utils/std/cpp20.h>
#include <nx/utils/std/cppnx.h>

namespace nx::utils::ranges {

struct AlwaysTrue
{
    template<typename... T>
    constexpr bool operator()(const T&...) const noexcept { return true; }
} constexpr alwaysTrue{};

template<typename Range, typename Element>
concept ViewOf = std::ranges::view<Range>
    && std::same_as<std::ranges::range_value_t<Range>, Element>;

/**
 * Reduce boilerplate when creating custom closures.
 * Example:
```
constexpr Closure spaceSplit =
    [](std::string_view v) -> std::vector<std::string_view>
    {
        return v | split(" "sv) | std::ranges::to<std::vector>();
    };

const std::vector words = "And his name is John Cena"sv | spaceSplit;
```
 */
template<class F>
struct Closure: std::ranges::range_adaptor_closure<Closure<F>>
{
    [[no_unique_address]] F f;

    constexpr Closure(F f): f(std::move(f)) {}

    template<typename... Args> requires std::invocable<const F&, Args...>
    constexpr auto operator()(Args&&... args) const &
    {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args> requires std::invocable<F&, Args...>
    constexpr auto operator()(Args&&... args) &
    {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args> requires std::invocable<F&, Args...>
    constexpr auto operator()(Args&&... args) &&
    {
        return std::invoke(std::move(f), std::forward<Args>(args)...);
    }

    template<typename... Args> requires(!std::invocable<const F&, Args...>)
    constexpr auto operator()(Args&&... args) const &
    {
        using BindT = std::decay_t<decltype(utils::bind_back(f, std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(f, std::forward<Args>(args)...)};
    }

    template<typename... Args> requires(!std::invocable<F&, Args...>)
    constexpr auto operator()(Args&&... args) && noexcept
    {
        using BindT = std::decay_t<decltype(utils::bind_back(std::move(f), std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(std::move(f), std::forward<Args>(args)...)};
    }
};

constexpr Closure split =
    []<std::ranges::viewable_range R>(R&& r, std::string_view Delim) -> ViewOf<std::string_view> auto
    {
        return std::forward<R&&>(r)
            | std::views::split(Delim)
            | std::views::transform(
                []<typename T>(T&& v) { return std::string_view(std::forward<T>(v)); })
            ;
    };

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

template<size_t N>
constexpr Closure to_tuple =
    []<std::ranges::viewable_range R>(R&& range)
    {
        auto it = std::ranges::begin(range);
        return
            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                return std::tuple{(void(I), *it++)...};
            }(std::make_index_sequence<N>{});
    };

constexpr Closure to_pair =
    []<std::ranges::viewable_range R,
        typename V = std::ranges::range_value_t<R>>(R&& range) -> std::pair<V, V>
    {
        auto it = std::ranges::begin(range);
        return
            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                return std::pair{(void(I), *it++)...};
            }(std::make_index_sequence<2>{});
    };

#ifdef NX_UTILS_NO_STD_RANGES_TO
    template <template <typename...> class To>
    constexpr auto to()
    {
        return Closure(
            []<typename R>(R&& r)
            {
                #if defined(__cpp_lib_containers_ranges)
                    return To(std::from_range, std::forward<R>(r));
                #else
                    // std::make_move_iterator will not compile with an iterator sentinel
                    auto c = std::forward<R>(r) | std::views::common;
                    return To(std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
                #endif
            });
    }
#else
    using std::ranges::to;
#endif

}  // namespace nx::utils::ranges
