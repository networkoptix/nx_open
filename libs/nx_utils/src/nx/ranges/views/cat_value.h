// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ranges>

namespace nx::views {
namespace detail {

template<std::ranges::range R>
    requires(std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
constexpr auto allOrOwning(R&& r)
{
    if constexpr (std::ranges::viewable_range<R>)
        return std::views::all(std::forward<R>(r));
    else
        return std::ranges::owning_view<std::remove_cvref_t<R>>(std::forward<R>(r));
}

template<typename T>
constexpr auto captureOne(T&& value)
{
    using U = T;

    if constexpr (std::is_lvalue_reference_v<T>)
    {
        // Preserve lvalues as references.
        return std::tuple<T>(value);
    }
    else if constexpr (std::constructible_from<std::remove_cvref_t<T>, T>)
    {
        // Materialize movable xvalues/prvalues.
        return std::tuple<std::remove_cvref_t<T>>(std::forward<T>(value));
    }
    else
    {
        // Preserve non-movable xvalues (e.g. const std::unique_ptr<int>&&) as xvalue refs.
        return std::tuple<U&&>(std::forward<T>(value));
    }
}

template<bool CatLeft, typename Bound, typename Tuple>
constexpr auto catTupleLike(const Bound& bound, Tuple&& t)
{
    return std::apply(
        [&]<typename... Ts>(Ts&&... elems)
        {
            if constexpr (CatLeft)
            {
                return std::tuple_cat(
                    std::make_tuple(bound), //< unwraps std::reference_wrapper
                    std::tuple_cat(captureOne(std::forward<Ts>(elems))...));
            }
            else
            {
                return std::tuple_cat(
                    std::tuple_cat(captureOne(std::forward<Ts>(elems))...),
                    std::make_tuple(bound)); //< unwraps std::reference_wrapper
            }
        },
        std::forward<Tuple>(t));
}

template<bool CatLeft, typename Bound, typename Elem>
constexpr auto catScalar(const Bound& bound, Elem&& e)
{
    if constexpr (CatLeft)
    {
        return std::tuple_cat(
            std::make_tuple(bound), // unwraps std::reference_wrapper
            captureOne(std::forward<Elem>(e)));
    }
    else
    {
        return std::tuple_cat(
            captureOne(std::forward<Elem>(e)),
            std::make_tuple(bound)); // unwraps std::reference_wrapper
    }
}

} // namespace detail

template<bool CatLeft, typename Value>
    requires std::copy_constructible<Value>
struct CatValueClosure
{
    Value value;

    /**
     * Lazily concatenates the bound value to every element of the input range.
     *
     * Behavior:
     * - If an element is tuple-like (`std::pair`, `std::tuple`, `std::array`, ...),
     *   the bound value is inserted on the left or right.
     * - Otherwise the element is treated as a scalar and the result is a 2-tuple.
     *
     * Value-category preservation:
     * - lvalues stay references;
     * - movable xvalues/prvalues are materialized by value;
     * - non-movable xvalues are preserved as xvalue references.
     *
     * The transformation is lazy and returns a `std::views::transform(...)` view.
     *
     * Example: tuple-like input, prepending a value
     * @code
     * std::cout << "\nowning tuple-like view via as_rvalue\n";
     * for (auto&& [serverName, deviceName, idPtr] :
     *     makeUniquePtrMap(33, 44)
     *     | std::views::as_rvalue
     *     | nx::views::catLeft("beta"sv))
     * {
     *     std::cout << serverName << " | " << deviceName << " | " << *idPtr << '\n';
     * }
     * @endcode
     *
     * Example: tuple-like input, appending a value
     * @code
     * for (auto&& [deviceName, idPtr, tag] :
     *     makeUniquePtrMap(33, 44)
     *     | std::views::as_rvalue
     *     | nx::views::catRight("beta"sv))
     * {
     *     std::cout << deviceName << " | " << *idPtr << " | " << tag << '\n';
     * }
     * @endcode
     *
     * Example: scalar input
     * @code
     * for (auto&& [tag, n] :
     *     std::array{1, 2, 3}
     *     | nx::views::catLeft("beta"sv))
     * {
     *     std::cout << tag << " | " << n << '\n';
     * }
     * @endcode
     */
    template<std::ranges::range R>
        requires(std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
    constexpr auto operator()(R&& r) const
    {
        return std::views::transform(
            detail::allOrOwning(std::forward<R>(r)),
            [bound = value]<typename E>(E&& e)
            {
                if constexpr (requires { typename std::tuple_size<std::remove_cvref_t<E>>::type; })
                    return detail::catTupleLike<CatLeft>(bound, std::forward<E>(e));
                else
                    return detail::catScalar<CatLeft>(bound, std::forward<E>(e));
            });
    }

    /**
     * Pipe syntax support.
     *
     * Equivalent to:
     * @code
     * self(std::forward<R>(range))
     * @endcode
     *
     * Example:
     * @code
     * auto view = range | nx::views::catLeft("beta"sv);
     * @endcode
     */
    template<std::ranges::range R>
        requires(std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
    friend constexpr auto operator|(R&& range, CatValueClosure self)
    {
        return self(std::forward<R>(range));
    }
};

/**
 * Returns a range-adaptor closure that prepends `value` to each element.
 *
 * The returned closure can be piped into a range:
 * @code
 * auto view = range | nx::views::catLeft("beta"sv);
 * @endcode
 *
 * Example with destructuring:
 * @code
 * for (auto&& [serverName, deviceName, idPtr] :
 *     makeUniquePtrMap(33, 44)
 *     | std::views::as_rvalue
 *     | nx::views::catLeft("beta"sv))
 * {
 *     std::cout << serverName << " | " << deviceName << " | " << *idPtr << '\n';
 * }
 * @endcode
 */
template<typename Value>
    requires std::copy_constructible<std::remove_cvref_t<Value>>
constexpr auto catLeft(Value&& value)
{
    using Stored = std::remove_cvref_t<Value>;
    return CatValueClosure<true, Stored>{Stored(std::forward<Value>(value))};
}

/**
 * Returns a range-adaptor closure that appends `value` to each element.
 *
 * The returned closure can be piped into a range:
 * @code
 * auto view = range | nx::views::catRight("beta"sv);
 * @endcode
 *
 * Example with destructuring:
 * @code
 * for (auto&& [deviceName, idPtr, tag] :
 *     makeUniquePtrMap(33, 44)
 *     | std::views::as_rvalue
 *     | nx::views::catRight("beta"sv))
 * {
 *     std::cout << deviceName << " | " << *idPtr << " | " << tag << '\n';
 * }
 * @endcode
 */
template<typename Value>
    requires std::copy_constructible<std::remove_cvref_t<Value>>
constexpr auto catRight(Value&& value)
{
    using Stored = std::remove_cvref_t<Value>;
    return CatValueClosure<false, Stored>{Stored(std::forward<Value>(value))};
}

/**
 * Immediate (non-adaptor) overload that prepends `value` to each element.
 *
 * Equivalent to:
 * @code
 * nx::views::catLeft(std::forward<Value>(value))(std::forward<R>(r))
 * @endcode
 *
 * Example:
 * @code
 * for (auto&& [serverName, deviceName, idPtr] :
 *     nx::views::catLeft(
 *         makeUniquePtrMap(33, 44) | std::views::as_rvalue,
 *         "beta"sv))
 * {
 *     std::cout << serverName << " | " << deviceName << " | " << *idPtr << '\n';
 * }
 * @endcode
 */
template<std::ranges::range R, typename Value>
    requires(std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
        && std::copy_constructible<std::remove_cvref_t<Value>>
constexpr auto catLeft(R&& r, Value&& value)
{
    return nx::views::catLeft(std::forward<Value>(value))(std::forward<R>(r));
}

/**
 * Immediate (non-adaptor) overload that appends `value` to each element.
 *
 * Equivalent to:
 * @code
 * nx::views::catRight(std::forward<Value>(value))(std::forward<R>(r))
 * @endcode
 *
 * Example:
 * @code
 * for (auto&& [deviceName, idPtr, tag] :
 *     nx::views::catRight(
 *         makeUniquePtrMap(33, 44) | std::views::as_rvalue,
 *         "beta"sv))
 * {
 *     std::cout << deviceName << " | " << *idPtr << " | " << tag << '\n';
 * }
 * @endcode
 */
template<std::ranges::range R, typename Value>
    requires(std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
        && std::copy_constructible<std::remove_cvref_t<Value>>
constexpr auto catRight(R&& r, Value&& value)
{
    return nx::views::catRight(std::forward<Value>(value))(std::forward<R>(r));
}

} // namespace nx::views
