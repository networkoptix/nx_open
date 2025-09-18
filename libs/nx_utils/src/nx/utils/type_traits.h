// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <string_view>
#include <type_traits>

namespace nx::traits {

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

} // namespace detail

//-------------------------------------------------------------------------------------------------

template<template<typename...> class Template, typename T>
constexpr bool is()
{
    return detail::Is_template<Template, T>{};
}

template<template<typename, size_t> class Template, typename T>
constexpr bool is()
{
    return detail::Is_template_type_size_t<Template, T>{};
}

template <typename T, template <typename...> class Template>
concept TemplateLike = is<Template, T>();

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

template <auto FunctionPtr, size_t I>
using FunctionArgumentType = typename FunctionTraits<FunctionPtr>::template ArgumentType<I>;

template<typename T>
concept ToStringViewConvertible = requires(const T& t) { std::string_view(t.data(), t.size()); };

} // namespace nx::traits
