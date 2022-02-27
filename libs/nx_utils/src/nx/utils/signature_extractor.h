// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once
#include <type_traits>
#include <tuple>

namespace nx {
namespace utils {

template <typename F>
struct SignatureExtractor;

template<typename Return, typename Class, typename... Args>
struct SignatureExtractor<Return (Class::*)(Args...)>
{
    using type = Return (Args...);
};

template<typename Return, typename Class, typename... Args>
struct SignatureExtractor<Return (Class::*)(Args...) const>
{
    using type = Return (Args...);
};

template<typename t, std::size_t n, typename = void> struct FunctionArgumentType;

/**
 * Helper trait object to extract type of Nth argument of a callable object.
 * Example usage:
 * ```
 * void someFunction(SomeClassA argument0, const SomeClassB& argument1);
 * ...
 * using A = FunctionArgumentType<decltype(someFunction), 0>::Type; // Maps to a type `SomeClassA`
 * using B = FunctionArgumentType<decltype(someFunction), 1>::Type; // Maps to a type `const SomeClassB&`
 * using RawB = FunctionArgumentType<decltype(someFunction), 1>::RawType; // Maps to a type `SomeClassB`
 * ```
 */
template<typename r, typename ... a, std::size_t n>
struct FunctionArgumentType<r (*)(a ...), n>
{
    using Type = typename std::tuple_element< n, std::tuple<a ...>>::type;
    using RawType = typename std::remove_cv<typename std::remove_reference<Type>::type>::type;
    using Return = r;
};

/** Specialization for a class method. */
template<typename r, typename c, typename ... a, std::size_t n>
struct FunctionArgumentType<r (c::*)(a ...), n>: FunctionArgumentType<r (*)(a ...), n>
{
};

/** Specialization for a const class method. */
template<typename r, typename c, typename ... a, std::size_t n>
struct FunctionArgumentType<r (c::*)(a ...) const, n>: FunctionArgumentType<r (c::*)(a ...), n>
{
};

/** Specialization for functor objects with `operator()`. */
template<typename functor, std::size_t n>
struct FunctionArgumentType<
    functor,
    n,
    typename std::conditional<false, decltype(& functor::operator ()), void>::type
>
: FunctionArgumentType<decltype(& functor::operator ()), n>
{
    using Functor = functor;
};

} // namespace utils
} // namespace nx
