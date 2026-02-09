// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <expected>
#include <ranges>
#include <variant>

#include <nx/concepts/common.h>
#include <nx/ranges/actions/append.h>
#include <nx/ranges/std_compat.h>

namespace nx::actions {
namespace detail {

template<typename S>
concept Sum = nx::SpecializationOf<S, std::expected> || nx::SpecializationOf<S, std::variant>;

template<typename S>
struct SumTraits;

template<typename V, typename Err>
struct SumTraits<std::expected<V, Err>>
{
    static constexpr std::size_t size = 2u;

    static constexpr std::size_t index(const std::expected<V, Err>& s) { return s ? 0u : 1u; }

    template<std::size_t I>
    using element_type = std::conditional_t<I == 0, V, Err>;

    template<std::size_t I, typename S>
    static constexpr decltype(auto) get(S&& s)
    {
        static_assert(I < 2u);

        if constexpr (I == 0)
            return *std::forward<S>(s);
        else
            return std::forward<S>(s).error();
    }
};

template<typename... Ts>
struct SumTraits<std::variant<Ts...>>
{
    static constexpr std::size_t size = sizeof...(Ts);

    static constexpr std::size_t index(const std::variant<Ts...>& s) { return s.index(); }

    template<std::size_t I>
    using element_type = std::variant_alternative_t<I, std::variant<Ts...>>;

    template<std::size_t I, typename S>
    static constexpr decltype(auto) get(S&& s)
    {
        return std::get<I>(std::forward<S>(s));
    }
};

template<Sum S>
constexpr std::size_t sum_size_v = SumTraits<S>::size;

template<Sum S>
constexpr std::size_t sumIndex(const S& s)
{
    return SumTraits<S>::index(s);
}

template<std::size_t I, Sum S>
using element_type_t = SumTraits<S>::template element_type<I>;

template<std::size_t I, Sum S>
constexpr decltype(auto) get(S&& s)
{
    using U = std::remove_cvref_t<S>;
    return SumTraits<U>::template get<I>(std::forward<S>(s));
}

} // namespace detail

/**
 * @brief Partition/materialize a range of "sum-like" elements into per-alternative containers.
 *
 * This is an *action* (materialization step): it consumes the input range once and returns
 * owning containers (no output views).
 *
 * Supported element types (E = std::ranges::range_value_t<R>):
 *
 *   1) std::expected<V, Err>
 *      Result type: std::tuple<MaterializeTo<V>, MaterializeTo<Err>>
 *        - bucket[0] receives all values (e.value())
 *        - bucket[1] receives all errors (e.error())
 *
 *   2) std::tuple<T0, T1, ...>
 *      Result type: std::tuple<MaterializeTo<T0>, MaterializeTo<T1>, ...>
 *        - bucket[i] receives all std::get<i>(tuple)
 *
 * Copy vs move policy:
 *   - If the input range is passed as a non-const rvalue (R&& where R is non-const),
 *     elements/components are moved into output containers (std::move).
 *   - Otherwise, elements/components are copied.
 *
 * Range requirements:
 *   - R models std::ranges::input_range (single-pass ranges / streams are supported).
 *
 * Container requirements:
 *   - MaterializeTo<T, ...> must be compatible with:
 *       nx::append_one(&container, value)
 *
 * Usage patterns:
 *
 *   A) Default materialization (std::vector) with pipe:
 *      auto out = range | nx::actions::partitionSums;
 *
 *   B) Default materialization (std::vector) with call:
 *      auto out = nx::actions::partitionSums(range);
 *
 *   C) Custom materialization container (e.g. std::set) with pipe:
 *      auto out = range | nx::actions::partitionSums_to<std::set>;
 *
 *   D) Custom materialization container (e.g. std::set) with call:
 *      auto out = nx::actions::partitionSumsTo<std::set>(range);
 *
 *   E) Enable move-path by passing the input range as an rvalue:
 *      auto out = nx::actions::partitionSums(std::move(container_range));
 *
 *   F) Stop early before materialization (single-pass friendly):
 *      // Example: input is a range of std::expected<V, Err>
 *      constexpr std::size_t kMaxErrors = 3;
 *      std::size_t errors = 0;
 *
 *      auto limited =
 *          input
 *          | std::views::take_while([&](auto const& e) {
 *                if (!e) ++errors;
 *                return errors <= kMaxErrors;
 *            });
 *
 *      auto out = limited | nx::actions::partitionSums;
 *
 * Notes:
 *   - For associative containers (e.g. std::set), ordering/deduplication semantics apply.
 *   - This action consumes the input exactly once; it is safe for single-pass ranges.
 */
template<template<typename, typename...> typename MaterializeTo>
struct PartitionSums: std::ranges::range_adaptor_closure<PartitionSums<MaterializeTo>>
{
    template<
        std::ranges::input_range R,
        typename E = std::remove_cvref_t<std::ranges::range_value_t<R>>>
        requires detail::Sum<E>
    constexpr auto operator()(R&& r) const
    {
        static constexpr bool movableRange =
            std::is_rvalue_reference_v<R&&> && !std::is_const_v<std::remove_reference_t<R>>;

        return std::invoke(
            [&]<std::size_t... Is>(std::index_sequence<Is...>)
            {
                return nx::ranges::fold_left(
                    std::forward<R>(r),
                    std::tuple<MaterializeTo<detail::element_type_t<Is, E>>...>{},
                    []<detail::Sum Elem>(auto acc, Elem&& elem)
                    {
                        if constexpr (movableRange)
                        {
                            ((Is == detail::sumIndex(elem)
                                  ? nx::actions::detail::appendOne(
                                        std::addressof(std::get<Is>(acc)),
                                        detail::get<Is>(std::forward<Elem>(elem)))
                                  : void()),
                             ...);
                        }
                        else
                        {
                            ((Is == detail::sumIndex(elem) ? nx::actions::detail::appendOne(
                                                                 std::addressof(std::get<Is>(acc)),
                                                                 detail::get<Is>(elem))
                                                           : void()),
                             ...);
                        }

                        return acc;
                    });
            },
            std::make_index_sequence<detail::sum_size_v<E>>{});
    }
};

/**
 * @brief Default partition action instance: materializes into std::vector.
 *
 * Example:
 *   auto [vals, errs] = input | nx::actions::partitionSums;
 */
constexpr PartitionSums<std::vector> partitionSums{};

/**
 * @brief Customizable partition action instance.
 *
 * Usage:
 *   // Pipe form:
 *   auto out = input | nx::actions::partitionSumsTo<std::set>;
 *
 *   // Call form:
 *   auto out = nx::actions::partitionSumsTo<std::set>(input);
 *
 * Example (std::expected):
 *   std::vector<std::expected<int, std::string>> input{1, unexpected("x"), 2, unexpected("y"), 2};
 *   auto [values, errors] = input | nx::actions::partitionSumsTo<std::set>;
 *   // values: {1,2}   errors: {"x","y"}
 *
 * Example (std::tuple):
 *   std::vector<std::tuple<int, std::string>> input{{1,"a"},{2,"b"},{2,"b"}};
 *   auto [ints, strs] = input | nx::actions::partitionSumsTo<std::set>;
 *   // ints: {1,2}   strs: {"a","b"}
 */
template<template<typename, typename...> typename MaterializeTo>
constexpr PartitionSums<MaterializeTo> partitionSumsTo{};

} // namespace nx::actions
