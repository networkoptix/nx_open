// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <ranges>

#include <nx/ranges/closure.h>

/*
Drop-in implementations for C++ standard library facilities that are declared in std, but not
implemented (or not fully implemented) by one or more compilers/standard libraries used in CI.

This header provides nx:: drop-ins with the same API/semantics as the corresponding std:: entities.

Once all CI toolchains provide working std:: implementations, remove the nx:: drop-ins and rename
all usages in the project from nx::... to std::....

It is illegal (undefined behavior) to declare new entities in namespace std, except for explicit
customizations permitted by the standard.
*/

namespace nx {
namespace ranges {

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
    struct ResolveType<
        To,
        ValueType,
        std::void_t<decltype(ValueType::first), decltype(ValueType::second)>>
    {
        using type =
            To<std::decay_t<decltype(std::declval<ValueType>().first)>,
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
                if constexpr (requires {
                                  To(std::make_move_iterator(c.begin()),
                                     std::make_move_iterator(c.end()));
                              })
                {
                    // This works for `to<std::map>()` and sometimes for `to<std::vector>()`
                    return To(std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
                }
                else
                {
                    using ValueType = std::ranges::range_value_t<R>;
                    typename detail::ResolveType<To, ValueType>::type result;

                    if constexpr (
                        std::ranges::sized_range<decltype(c)>
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
                                    static_assert(
                                        b,
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
                if constexpr (requires {
                                  Container(
                                      std::make_move_iterator(c.begin()),
                                      std::make_move_iterator(c.end()));
                              })
                {
                    return Container(
                        std::make_move_iterator(c.begin()),
                        std::make_move_iterator(c.end()));
                }

                Container result{};
                if constexpr (
                    std::ranges::sized_range<decltype(c)>
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
                                    b,
                                    "The container type has no recognised insertion member");
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
    template<std::ranges::forward_range R, typename Accum, typename BinaryOp>
    constexpr auto fold_left(R&& r, Accum&& init, BinaryOp f)
    {
        auto c = std::views::common(std::forward<R>(r));
        return std::accumulate(
            std::ranges::begin(c),
            std::ranges::end(c),
            std::forward<Accum>(init),
            std::ref(f));
    }
#endif

} // namespace ranges

namespace views {

// FIXME: #skolesnik std::views::cartesian_product is in views namespace. The drop-in replacement
// may not be compatible.
#if defined __cpp_lib_ranges_cartesian_product
    using std::views::cartesian_product;
#else
    namespace detail {

    template<
        std::size_t Depth = 0,
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

} // namespace views
} // namespace nx
