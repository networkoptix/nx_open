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

template<typename T, typename Sig>
struct CallableImpl: std::false_type
{
};

template<typename T, typename R, typename... Args>
struct CallableImpl<T, R(Args...)>: std::bool_constant<std::is_invocable_r_v<R, T&, Args...>>
{
};

template<typename T, typename R, typename... Args>
struct CallableImpl<T, R(Args...) noexcept>:
    std::bool_constant<std::is_nothrow_invocable_r_v<R, T&, Args...>>
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

template<typename T>
concept ToStringViewConvertible = requires(const T& t) { std::string_view(t.data(), t.size()); };

/*
    Callable<T, Sig>

    Sig is a function type such as int(double, const Foo&).

    Example: use directly in the template parameter list

        template<Callable<int(double, const Foo&)> F>
        int invoke(F&& f, double x, const Foo& y)
        {
            return f(x, y);
        }

    Example: use in if constexpr

        template<class F>
        void dispatch(F&& f, double x, const Foo& y)
        {
            if constexpr (Callable<F, int(double, const Foo&)>) {
                (void)f(x, y);
            } else {
                // fallback
            }
        }

    Notes:
    - Uses normal invocability rules, so implicit argument conversions are allowed.
    - For noexcept callability, use a noexcept function type, e.g.
      int(double, const Foo&) noexcept.
*/
template<typename T, typename Sig>
concept Callable = detail::CallableImpl<T, Sig>::value;

} // namespace nx
