// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <iterator>
#include <numeric>
#include <ranges>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <nx/utils/std/functional.h>

namespace nx {

// TODO: Move somewhere else.
struct AlwaysTrue
{
    template<typename... T>
    constexpr bool operator()(const T&...) const noexcept { return true; }
} constexpr alwaysTrue{};

struct AlwaysFalse
{
    template<typename... T>
    constexpr bool operator()(const T&...) const noexcept { return false; }
} constexpr alwaysFalse{};

// TODO: #skolesnik
// Some of the adapted for piping std::ranges operations, are available out-of-the box in
// range-v3 as 'action' , i.e. ranges::action::sort.
// ranges-v3 also provide somewhat-convenient interface for implementing custom closures, but not
// as convenient as Closure class below.

// FIXME: #skolesnik
// Fix overload resolution for argument-binding Closure::operator(). As of this writing it gives
// false positive on things such `is_invocable_v<decltype(Closure{[]{}), int, int>`, hence can't
// be used in functions like `ServerForTests::executeGet()`.
/**
 * Reduce boilerplate when creating custom closures.
 * Allows to pipe non-ranges types.
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
template<typename F>
struct Closure
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

    template<typename... Args>
        requires std::invocable<const F&, Args...>
    constexpr auto operator()(Args&&... args) const& {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires std::invocable<F&, Args...>
    constexpr auto operator()(Args&&... args) &
    {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires std::invocable<F&&, Args...>
    constexpr auto operator()(Args&&... args) &&
    {
        return std::invoke(std::move(f), std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires(!std::invocable<const F&, Args...>)
    constexpr auto operator()(Args&&... args) const&
    {
        using BindT = std::decay_t<decltype(utils::bind_back(f, std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(f, std::forward<Args>(args)...)};
    }

    template<typename... Args>
        requires(!std::invocable<F&, Args...>)
    constexpr auto operator()(Args&&... args) && noexcept
    {
        using BindT =
            std::decay_t<decltype(utils::bind_back(std::move(f), std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(std::move(f), std::forward<Args>(args)...)};
    }
};

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, const Closure<F>& f)
    noexcept(noexcept(f(std::forward<T>(v))))
    requires requires { f(std::forward<T>(v)); }
{
    return f(std::forward<T>(v));
}

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, Closure<F>& f)
    noexcept(noexcept(f(std::forward<T>(v))))
    requires requires { f(std::forward<T>(v)); }
{
    return f(std::forward<T>(v));
}

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, Closure<F>&& f)
    noexcept(noexcept(std::move(f)(std::forward<T>(v))))
    requires requires { std::move(f)(std::forward<T>(v)); }
{
    return std::move(f)(std::forward<T>(v));
}

namespace ranges {

template<typename Range, typename Element>
concept ViewOf =
    std::ranges::view<Range>
    && std::same_as<std::ranges::range_value_t<Range>, Element>;

}

// -- Operations with Lazy evaluation, returning a View -----------------------------------------
namespace views {

constexpr Closure split =
    []<std::ranges::viewable_range R>(R&& r, std::string_view Delim)
        -> nx::ranges::ViewOf<std::string_view> auto
    {
        return std::forward<R>(r)
            | std::views::split(Delim)
            | std::views::transform(
                []<typename T>(T&& v) { return std::string_view(std::forward<T>(v)); });
    };

} // namespace nx::views

// -- Operations with Eager evaluation ----------------------------------------------------------
namespace ranges {

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

constexpr Closure sort =
    []<std::ranges::random_access_range R, typename Comp = std::less<>>(R&& r, Comp comp = {})
        requires std::sortable<std::ranges::iterator_t<R>, Comp>
    {
        std::ranges::sort(r, comp);
        return std::forward<R>(r);
    };

// Shrinks a container if it has erase(), otherwise returns a trimmed subrange.
constexpr Closure unique =
    []<std::ranges::forward_range R, typename Pred = std::ranges::equal_to>(R&& r, Pred pred = {})
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

// TODO: #skolesnik transform_to<std::set/std::map/...> (select container type)
// TODO: #skolesnik transform -> retain container type
// TODO: #skolesnik transform with in-place modification
// TODO: #skolesnik Investigate if range-v3 counterpart provides everything and is easy to use
constexpr Closure transform =
    []<std::ranges::forward_range R, typename Pred>
        requires std::invocable<Pred, std::ranges::range_value_t<R>>
        (R&& r, Pred pred)
    {
        std::vector<std::invoke_result_t<Pred&, std::ranges::range_value_t<R>>> result;
        if constexpr (std::ranges::sized_range<R>)
            result.reserve(std::ranges::size(r));

        for (auto&& e: r)
            result.emplace_back(pred(std::move(e)));

        return result;
    };

#if defined __cpp_lib_ranges_to_container
    using std::ranges::to;
#else
    namespace detail {

    template<template<typename...> typename To, typename ValueType, typename = void>
    struct ResolveType
    {
        using type = To<ValueType>;
    };

    template<template<typename...> typename To, typename ValueType>
    struct ResolveType<To,
        ValueType,
        std::void_t<decltype(ValueType::first), decltype(ValueType::second)>>
    {
        using type = To<std::decay_t<decltype(std::declval<ValueType>().first)>,
            std::decay_t<decltype(std::declval<ValueType>().second)>>;
    };

    } // namespace detail

    // Limited adapter
    template<template<typename...> typename To>
    constexpr auto to()
    {
        return Closure(
            []<typename R>(R&& r)
            {
                #if defined __cpp_lib_containers_ranges
                    return To(std::from_range, std::forward<R>(r));
                #else
                    auto c = std::forward<R>(r) | std::views::common;
                    if constexpr (
                        requires { To(std::make_move_iterator(c.begin()),
                            std::make_move_iterator(c.end())); })
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
                            {
                                result.emplace_back(std::move(v));
                            }
                            else if constexpr (requires { result.push_back(std::move(v)); })
                            {
                                result.push_back(std::move(v));
                            }
                            else if constexpr (requires { result.emplace(std::move(v)); })
                            {
                                result.emplace(std::move(v));
                            }
                            else if constexpr (requires { result.insert(std::move(v)); })
                            {
                                result.insert(std::move(v));
                            }
                            else
                            {
                                // Ad hoc
                                std::invoke(
                                    []<bool b = false>
                                    {
                                        static_assert(b,
                                            "The container type has no recognised insertion member");
                                    });
                            }
                        }
                        return result;
                    }
                #endif
            });
    }

    // Overload for concrete container types (e.g., std::string, std::vector<std::string>).
    template<typename Container>
    constexpr auto to()
    {
        return Closure(
            []<typename R>(R&& r)
            {
                #if defined __cpp_lib_containers_ranges
                    if constexpr (requires { Container(std::from_range, std::forward<R>(r)); })
                        return Container(std::from_range, std::forward<R>(r));
                #else
                    auto c = std::forward<R>(r) | std::views::common;

                    // Fixes std::string from range of char
                    if constexpr (requires { Container(c.begin(), c.end()); })
                        return Container(c.begin(), c.end());

                    // Moves for non-char types:
                    if constexpr (
                        requires { Container(std::make_move_iterator(c.begin()),
                            std::make_move_iterator(c.end())); })
                    {
                        return Container(
                            std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
                    }

                    Container result{};
                    if constexpr (std::ranges::sized_range<decltype(c)>
                        && requires(Container& t, std::size_t n) { t.reserve(n); })
                    {
                        result.reserve(std::ranges::size(c));
                    }

                    for (auto&& v: c)
                    {
                        using U = std::decay_t<decltype(v)>;

                        if constexpr (requires { result.append(std::forward<U>(v)); })
                        {
                            result.append(std::move(v));
                        }
                        else if constexpr (requires { result.emplace_back(std::forward<U>(v)); })
                        {
                            result.emplace_back(std::forward<U>(v));
                        }
                        else if constexpr (requires { result.push_back(std::forward<U>(v)); })
                        {
                            result.push_back(std::forward<U>(v));
                        }
                        else if constexpr (requires { result.emplace(std::forward<U>(v)); })
                        {
                            result.emplace(std::forward<U>(v));
                        }
                        else if constexpr (requires { result.insert(std::forward<U>(v)); })
                        {
                            result.insert(std::forward<U>(v));
                        }
                        else
                        {
                            // Ad hoc
                            std::invoke(
                                []<bool b = false>
                                {
                                    static_assert(
                                        b, "The container type has no recognised insertion member");
                                });
                        }
                    }
                    return result;
                #endif
            });
    }
#endif

#if defined __cpp_lib_ranges_fold
    using std::ranges::fold_left;
#else
    constexpr Closure fold_left =
        []<std::ranges::forward_range R, typename Accum, typename BinaryOp>
            (R&& r, Accum&& init, BinaryOp f)
        {
            auto c = std::forward<R>(r) | std::views::common;

            return std::accumulate(std::ranges::begin(c), std::ranges::end(c),
                std::forward<Accum>(init), std::ref(f));
        };
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

/**
 * Returns two vectors: [first, mid) with elements satisfying pred, and [mid, last) with the rest.
 */
constexpr Closure partition =
    // TODO: #skolesnik More generic version
    []<typename T, typename Pred>(std::vector<T> v, Pred&& pred) -> std::array<std::vector<T>, 2>
    {
        auto first = v.begin();
        auto last = v.end();

        // ranges::partition returns subrange [pivot, last)
        auto sub = std::ranges::partition(first, last, std::forward<Pred>(pred));
        auto pivot = sub.begin();

        return {
            std::vector<T>(std::make_move_iterator(first), std::make_move_iterator(pivot)),
            std::vector<T>(std::make_move_iterator(pivot), std::make_move_iterator(last)),
        };
    };

/**
 * Partitions std::vector<std::variant<T...>> into std::tuple<std::vector<T>...>. Moves each
 * element to its matching bucket; per-type order is preserved.
 */
constexpr Closure partition_sums =
    []<typename... T>(std::vector<std::variant<T...>> v) -> std::tuple<std::vector<T>...>
    {
        return nx::ranges::fold_left(std::move(v),
            std::tuple<std::vector<T>...>{},
            [](auto acc, auto& elem)
            {
                // Dispatch by the runtime variant index to the compile-time tuple index
                [&]<std::size_t... Is>(std::index_sequence<Is...>)
                {
                    ((Is == elem.index()
                             ? ((void) std::get<Is>(acc).emplace_back(std::get<Is>(std::move(elem))),
                                   void())
                             : void()),
                        ...);
                }(std::index_sequence_for<T...>{});
                return acc;
            });
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

} // namespace ranges
} // namespace nx
