// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <type_traits>

namespace nx::utils {

namespace detail {

template<template<typename...> class, typename>
struct Is_template: std::false_type
{
};

template<template<typename...> class T, typename... Ts>
struct Is_template<T, T<Ts...>>: std::true_type
{
};

template<template<typename, size_t> class, typename>
struct Is_template_type_size_t: std::false_type
{
};

template<template<typename, size_t> class T, typename F, size_t S>
struct Is_template_type_size_t<T, T<F, S>>: std::true_type
{
};

// using nx::utils::model::getId;
// using nx::utils::model::setId;

} // namespace detail

template<typename T, typename = void>
struct isIterable: std::false_type {};

template<typename T>
struct isIterable<
    T,
    std::void_t<
        decltype(std::begin(std::declval<T>())),
        decltype(std::end(std::declval<T>()))
    >
>: std::true_type {};

template<typename... U> inline constexpr bool IsIterableV = isIterable<U...>::value;

//-------------------------------------------------------------------------------------------------

template<typename T, typename = void>
struct IsIterator: std::false_type {};

template<typename T>
struct IsIterator<T, std::void_t<typename std::iterator_traits<T>::value_type>>: std::true_type {};

template<typename... U> inline constexpr bool IsIteratorV = IsIterator<U...>::value;

template<template<typename...> class Template, typename T>
constexpr bool Is()
{
    return detail::Is_template<Template, T>{};
}

template<template<typename, size_t> class Template, typename T>
constexpr bool Is()
{
    return detail::Is_template_type_size_t<Template, T>{};
}

template<typename T, typename = void>
struct IsKeyValueContainer: std::false_type
{
};

template<typename T>
struct IsKeyValueContainer<T,
    std::void_t<decltype(std::declval<T>().begin()->first),
        decltype(std::declval<T>().begin()->second)>>: std::true_type
{
};

template<auto>
struct FunctionTraits;

// For free and static member functions
template<typename Ret, typename... Args, Ret (*FuncPtr)(Args...)>
struct FunctionTraits<FuncPtr>
{
    using ReturnType = Ret;
    using ArgumentsList = std::tuple<Args...>;

    template<size_t I>
    using ArgumentType = std::tuple_element_t<I, ArgumentsList>;
};

// For non-static member functions
template<typename Ret, typename Class, typename... Args, Ret (Class::*MemberPtr)(Args...)>
struct FunctionTraits<MemberPtr>
{
    using ReturnType = Ret;
    using ArgumentsList = std::tuple<Args...>;

    template<size_t I>
    using ArgumentType = std::tuple_element_t<I, ArgumentsList>;
};

} // namespace nx::utils

namespace QnTypeTraits {

    struct na {};

    template<class T>
    struct identity {
        typedef T type;
    };

    template<class T>
    struct remove_cvr:
        std::remove_cv<
            typename std::remove_reference<T>::type
        > {};

    struct yes_type { char dummy; };
    struct no_type { char dummy[64]; };

} // namespace QnTypeTraits
