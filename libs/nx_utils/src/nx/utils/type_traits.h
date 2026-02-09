// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <nx/concepts.h>

namespace nx::traits {

// 'Polymorphic' overload for 'regular' Template. C++ doesn't allow specializing concepts.
template<template<typename...> class Template, typename T>
consteval bool is()
{
    return nx::SpecializationOf<T, Template>;
}

// 'Polymorphic' overload for 'sized' Template. C++ doesn't allow specializing concepts.
template<template<typename, size_t> class Template, typename T>
consteval bool is()
{
    return nx::SizedSpecializationOf<T, Template>;
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

} // namespace nx::traits
