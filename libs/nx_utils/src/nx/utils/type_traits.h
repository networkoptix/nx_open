// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <string_view>
#include <type_traits>

namespace nx {

namespace detail {

template<template<typename...> class Template, typename T>
struct IsSpecializationOf: std::false_type
{
};

template<template<typename...> class Template, typename... Deduced>
struct IsSpecializationOf<Template, Template<Deduced...>>: std::true_type
{
};

template<template<typename, std::size_t> class Template, typename T>
struct IsSizedSpecializationOf: std::false_type
{
};

template<template<typename, std::size_t> class Template, typename Deduced, std::size_t S>
struct IsSizedSpecializationOf<Template, Template<Deduced, S>>: std::true_type
{
};

} // namespace detail

// Matches any specialization of `Template`:
//
//   template <SpecializationOf<std::vector> Vec>
//   void foo(const Vec&);
//
//   template <SpecializationOf<std::map> Map>
//   void bar(const Map&);
//
// NOTE: T must come first, so as constraint it looks like `template <SpecializationOf<std::vector>
// T>`. But if used as an expression, it will be not as neat: `static_assert(SpecializationOf<T,
// std::optional>)`.
template<typename T, template<typename...> class Template>
concept SpecializationOf = detail::IsSpecializationOf<Template, std::remove_cvref_t<T>>::value;

// Matches any specialization of `Template` with `size_t` as a second arg:
//
//   template <SizedSpecializationOf<std::array> Array>
//   void baz(const Array&);
template<typename T, template<typename, std::size_t> class Template>
concept SizedSpecializationOf =
    detail::IsSizedSpecializationOf<Template, std::remove_cvref_t<T>>::value;

template<typename T>
concept TimePeriodLike = requires(const T& t) {
    { t.startTimeMs } -> std::convertible_to<std::chrono::milliseconds>;
    { t.durationMs } -> std::convertible_to<std::chrono::milliseconds>;
};

// TODO: #skolesnik This namespace adds unnecessary verbosity
namespace traits {

// 'Polymorphic' overload for 'regular' Template. C++ doesn't allow specializing concepts.
template<template<typename...> class Template, typename T>
consteval bool is()
{
    return SpecializationOf<T, Template>;
}

// 'Polymorphic' overload for 'sized' Template. C++ doesn't allow specializing concepts.
template<template<typename, size_t> class Template, typename T>
consteval bool is()
{
    return SizedSpecializationOf<T, Template>;
}

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

template<auto FunctionPtr, size_t I>
using FunctionArgumentType = typename FunctionTraits<FunctionPtr>::template ArgumentType<I>;

template<typename T>
concept ToStringViewConvertible = requires(const T& t) { std::string_view(t.data(), t.size()); };

} // namespace traits
} // namespace nx
