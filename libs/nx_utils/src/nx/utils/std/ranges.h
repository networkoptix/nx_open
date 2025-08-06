// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
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

// FIXME: #skolesnik
// Fix overload resolution for argument-binding Closure::operator(). As of this writing it gives
// false positive on things such `is_invocable_v<decltype(Closure{[]{}), int, int>`, hence can't
// be used in functions like `ServerForTests::executeGet()`.
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
    // Microsoft's way to "support" the standard's [[no_unique_address]] attribute is
    // "It won't create an error, but it won't have any effect":
    // https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/
    //
    // To ensure ABI compatibility with MSVC code, clang doesn't support that attribute either
    // when compiling for Windows target, reporting a warning/error for the "unknown" attribute.
    //
    // Meanwhile, both compilers support an extension attribute with the same functionality.
    #ifdef _MSC_VER
    [[msvc::no_unique_address]] F f;
    #else
    [[no_unique_address]] F f;
    #endif

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
        // TODO: #skolesnik std::views::adjacent (apparently does the same thing, but returns a view)
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

constexpr Closure sort =
    []<std::ranges::random_access_range R, class Comp = std::less<>>
        (R&& r, Comp comp = {})
        requires std::sortable<std::ranges::iterator_t<R>, Comp>
    {
        std::ranges::sort(r, comp);
        return std::forward<R>(r);
    };

// Shrinks a container if it has erase(), otherwise returns a trimmed subrange.
constexpr Closure unique = //
    []<std::ranges::forward_range R, class Pred = std::ranges::equal_to>
        (R&& r, Pred pred = {})
        requires std::permutable<std::ranges::iterator_t<R>>
    {
        auto res = std::ranges::unique(r, pred);
        auto newEnd = res.begin();

        if constexpr (auto oldEnd = res.end(); requires(R& c) { c.erase(newEnd, oldEnd); })
        {
            // It's a container with erase()
            r.erase(newEnd, oldEnd);
            return std::forward<R>(r);
        }
        else
        {
            // It's a view: return the deduplicated prefix
            return std::ranges::subrange(std::ranges::begin(r), newEnd);
        }
    };

#ifdef NX_UTILS_NO_STD_RANGES_TO
    namespace detail {

        template<template<typename...> class To, typename ValueType, typename = void>
        struct ResolveType
        {
            using type = To<ValueType>;
        };

        template<template<typename...> class To, typename ValueType>
        struct ResolveType<To,
            ValueType,
            std::void_t<decltype(ValueType::first), decltype(ValueType::second)>>
        {
            using type = To<std::decay_t<decltype(std::declval<ValueType>().first)>,
                std::decay_t<decltype(std::declval<ValueType>().second)>>;
        };

    } // namespace detail

    // Limited adapter
    template <template <typename...> class To>
    constexpr auto to()
    {
        return Closure(
            []<typename R>(R&& r)
            {
                #if defined(__cpp_lib_containers_ranges)
                    return To(std::from_range, std::forward<R>(r));
                #else
                    auto c = std::forward<R>(r) | std::views::common;
                    if constexpr (requires { To(std::make_move_iterator(c.begin()), std::make_move_iterator(c.end())); })
                    {
                        // This works for `to<std::map>()` and sometimes for `to<std::vector>()`
                        return To(std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
                    }
                    else
                    {
                        using ValueType = std::ranges::range_value_t<R>;
                        typename detail::ResolveType<To, ValueType>::type result;

                        if constexpr (std::ranges::sized_range<decltype(c)>
                            && requires(To<ValueType> t, std::size_t n) { t.reserve(n); })
                        {
                            result.reserve(std::ranges::size(c));
                        }

                        for (auto&& v: c)
                        {
                            if constexpr (requires { result.emplace_back(std::move(v)); })
                                result.emplace_back(std::move(v));
                            else if constexpr (requires { result.push_back(std::move(v)); })
                                result.push_back(std::move(v));
                            else if constexpr (requires { result.emplace(std::move(v)); })
                                result.emplace(std::move(v));
                            else if constexpr (requires { result.insert(std::move(v)); })
                                result.insert(std::move(v));
                            else
                            {
                                static_assert([] { return false; }(),
                                    "The container type has no recognised insertion member");
                            }
                        }
                        return result;
                    }
                #endif
            });
    }
#else
    using std::ranges::to;
#endif

#if defined __cpp_lib_ranges_cartesian_product
    using std::views::cartesian_product;
#else

    namespace detail {

    template<std::size_t Depth = 0,
        typename Row,
        std::ranges::input_range Head,
        std::ranges::input_range... Tail>
    void cartesian_product(std::vector<Row>* accumulator, Row* currentRow, Head&& head, Tail&&... tail)
    {
        for (auto&& item: head)
        {
            std::get<Depth>(*currentRow) = std::forward<decltype(item)>(item);

            if constexpr (sizeof...(tail) == 0)
                accumulator->emplace_back(std::move(*currentRow));
            else
                cartesian_product<Depth + 1>(accumulator, currentRow, std::forward<Tail>(tail)...);
        }
    }

    } // namespace detail

    template<std::ranges::input_range... Ranges>
    using CartesianProductRow = std::tuple<std::ranges::range_value_t<Ranges>...>;

    template<std::ranges::input_range... Ranges>
    using CartesianProductResultVector = std::vector<CartesianProductRow<Ranges...>>;

    // Drop in replacement for `std::views::cartesian_product`, but not a view (not lazy)
    template<std::ranges::input_range... Ranges>
    CartesianProductResultVector<Ranges...> cartesian_product(Ranges&&... ranges)
    {
        static_assert(sizeof...(Ranges) > 0, "cartesian_product requires at least one range");

        CartesianProductResultVector<Ranges...> result;

        if constexpr ((std::ranges::sized_range<Ranges> && ...))
        {
            std::size_t cap = 1;
            ((cap *= std::ranges::size(ranges)), ...);
            result.reserve(cap);
        }

        CartesianProductRow<Ranges...> currentRow;
        detail::cartesian_product<0>(&result, &currentRow, std::forward<Ranges>(ranges)...);

        return result;
    }

#endif

}  // namespace nx::utils::ranges
