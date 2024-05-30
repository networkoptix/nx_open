// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "type_traits.h"

namespace nx {
namespace utils {

namespace detail {

template<size_t N>
struct Apply
{
    template<typename Func, typename Tuple, typename... FuncArgs>
    static auto apply(
        Func&& func,
        Tuple&& tuple,
        /* for recursion only */ FuncArgs&&... funcArgs)
    {
        return Apply<N - 1>::apply(
            ::std::forward<Func>(func),
            ::std::forward<Tuple>(tuple),
            ::std::get<N - 1>(::std::forward<Tuple>(tuple)),
            ::std::forward<FuncArgs>(funcArgs)...);
    }
};

template<>
struct Apply<0>
{
    template<typename Func, typename Tuple, typename... FuncArgs>
    static auto apply(Func&& func, Tuple&& /*tuple*/, FuncArgs&&... funcArgs)
    {
        return ::std::forward<Func>(func)(::std::forward<FuncArgs>(funcArgs)...);
    }
};

template<typename Func, typename... Args, size_t... Index>
void tuple_for_each_impl(
    Func func,
    const std::tuple<Args...>& tuple,
    std::index_sequence<Index...>)
{
    /*unused*/ (void) std::initializer_list<int>{
        (func(std::get<Index>(tuple)) /*operator comma*/, 1)...};
}

} // namespace detail

//-------------------------------------------------------------------------------------------------

/** Calls func(), supplying the members of `tuple` as arguments. */
template<typename Func, typename Tuple>
inline auto expandTupleIntoArgs(Func&& func, Tuple&& tuple)
{
    return
        detail::Apply<
            ::std::tuple_size<typename ::std::decay<Tuple>::type>::value
        >::apply(
            ::std::forward<Func>(func), ::std::forward<Tuple>(tuple)
        );
}

/**
 * NOTE: Tuple elements are iterated forward.
 */
template<typename Func, typename... Args>
void tuple_for_each(const std::tuple<Args...>& tuple, Func func)
{
    return detail::tuple_for_each_impl(
        func,
        tuple,
        std::make_index_sequence<sizeof...(Args)>{});
}

/**
 * Invokes func with each argument of Args pack separately.
 * NOTE: Args packs elements are iterated forward.
 */
template<typename Func, typename... Args>
inline void apply_each(Func&& func, Args&&... args)
{
    /*unused*/ (void) std::initializer_list<int>{(func(std::forward<Args>(args)), 1)...};
}

/**
 * Converts unique_ptr of one type to another using static_cast.
 * Consumes original unique_ptr. Actually moves memory to a new unique_ptr.
 */
template<typename ResultType, typename InitialType, typename DeleterType>
std::unique_ptr<ResultType, DeleterType>
    static_unique_ptr_cast(std::unique_ptr<InitialType, DeleterType>&& sourcePtr)
{
    return std::unique_ptr<ResultType, DeleterType>(
        static_cast<ResultType>(sourcePtr.release()),
        sourcePtr.get_deleter());
}

template<typename ResultType, typename InitialType>
std::unique_ptr<ResultType, std::default_delete<ResultType>>
    static_unique_ptr_cast(std::unique_ptr<InitialType, std::default_delete<InitialType>>&& sourcePtr)
{
    return std::unique_ptr<ResultType>(
        static_cast<ResultType*>(sourcePtr.release()));
}

template<typename Object, typename Deleter>
std::unique_ptr<Object, Deleter> wrapUnique(Object* ptr, Deleter deleter)
{
    return std::unique_ptr<Object, Deleter>(ptr, std::move(deleter));
}

template<typename Object, typename Deleter>
std::shared_ptr<Object> wrapShared(Object* ptr, Deleter deleter)
{
    return std::shared_ptr<Object>(ptr, std::move(deleter));
}

//-------------------------------------------------------------------------------------------------

/**
 * Defines type that is set to the type of the first element of Tuple or to the DefaultType if
 * Tuple is empty.
 */
template<typename Tuple, typename DefaultType = void> struct tuple_first_element;

template<typename Head, typename DefaultType, typename... Args>
struct tuple_first_element<std::tuple<Head, Args...>, DefaultType>
{
    using type = Head;
};

template<typename DefaultType>
struct tuple_first_element<std::tuple<>, DefaultType>
{
    using type = DefaultType;
};

template<typename... Args>
using tuple_first_element_t = typename tuple_first_element<Args...>::type;

//-------------------------------------------------------------------------------------------------

/**
 * Defines type that is set to the type of the last element type of Tuple or to the DefaultType if
 * Tuple is empty.
 */
template<typename Tuple, typename DefaultType = void>
struct tuple_last_element
{
    using type = typename std::tuple_element<
        std::tuple_size<Tuple>::value - 1, Tuple>::type;
};

template<typename DefaultType>
struct tuple_last_element<std::tuple<>, DefaultType>
{
    using type = DefaultType;
};

template<typename... Args>
using tuple_last_element_t = typename tuple_last_element<Args...>::type;

//-------------------------------------------------------------------------------------------------

namespace detail {

template<std::size_t pos, typename Tuple, std::size_t... I>
auto make_tuple_from_arg_range(Tuple&& tuple, std::index_sequence<I...>)
{
    return std::make_tuple(
        std::get<pos + I>(std::forward<Tuple>(tuple))...);
}

} // namespace detail

/**
 * @return std::tuple consisting of count types of Args starting with pos.
 * E.g., let (Args... args) be the following variadic pack: {1, "Hello", std::vector<int>()};
 * Then make_tuple<1, 1>(args...) returns std::make_tuple("Hello");
 * NOTE: Arguments that are out of [pos; pos+count) range are not modified even if passed as T&&.
 */
template<std::size_t pos, std::size_t count, typename... Args>
auto make_tuple_from_arg_range(Args&&... args)
{
    static_assert(pos + count <= sizeof...(Args));

    using Indices = std::make_index_sequence<count>;

    return detail::make_tuple_from_arg_range<pos>(
        std::forward_as_tuple(std::forward<Args>(args)...),
        Indices{});
}

//-------------------------------------------------------------------------------------------------

template<typename T>
struct identity
{
    using type = T;
};

//-------------------------------------------------------------------------------------------------
// String type utils.

template<typename T, typename = void>
struct IsConvertibleToStringView: std::false_type {};

template<typename T>
struct IsConvertibleToStringView<
    T,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<const T>().data()), const char*> &&
        std::is_integral_v<decltype(std::declval<T>().size())>>
    >
    :
    std::true_type
{
};

template<typename... U> inline constexpr bool IsConvertibleToStringViewV =
    IsConvertibleToStringView<U...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = void>
struct HasToString: std::false_type {};

template<typename T>
struct HasToString<
    T,
    std::enable_if_t<
        std::is_same_v<decltype(std::declval<T>().toString()), std::string>>
    >: public std::true_type
{
};

template<typename... U>
inline constexpr bool HasToStringV = HasToString<U...>::value;

} // namespace utils
} // namespace nx
