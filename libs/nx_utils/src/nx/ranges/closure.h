// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/general_macros.h>
#include <nx/utils/std/functional.h>

namespace nx {

// FIXME: #skolesnik
// Fix overload resolution for argument-binding Closure::operator(). As of this writing it gives
// false positive on things such `is_invocable_v<decltype(Closure{[]{}), int, int>`, hence can't
// be used in functions like `ServerForTests::executeGet()`.
/**
 * Reduce boilerplate when creating custom closures.
 * Allows to pipe non-ranges types.
 * Example:
```
constexpr Closure spaceSplit =
    [](std::string_view v) -> std::vector<std::string_view>
    {
        return v | split(" "sv) | std::ranges::to<std::vector>();
    };

const std::vector words = "And his name is John Cena"sv | spaceSplit;
```
 */
template<typename F>
struct Closure
{
    NX_NO_UNIQUE_ADDRESS F f;

    constexpr Closure(F f):
        f(std::move(f))
    {
    }

    template<typename... Args>
        requires std::invocable<const F&, Args...>
    constexpr auto operator()(Args&&... args) const&
    {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires std::invocable<F&, Args...>
    constexpr auto operator()(Args&&... args) &
    {
        return std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires std::invocable<F&&, Args...>
    constexpr auto operator()(Args&&... args) &&
    {
        return std::invoke(std::move(f), std::forward<Args>(args)...);
    }

    template<typename... Args>
        requires(!std::invocable<const F&, Args...>)
    constexpr auto operator()(Args&&... args) const&
    {
        using BindT = std::decay_t<decltype(utils::bind_back(f, std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(f, std::forward<Args>(args)...)};
    }

    template<typename... Args>
        requires(!std::invocable<F&, Args...>)
    constexpr auto operator()(Args&&... args) && noexcept
    {
        using BindT =
            std::decay_t<decltype(utils::bind_back(std::move(f), std::forward<Args>(args)...))>;
        return Closure<BindT>{utils::bind_back(std::move(f), std::forward<Args>(args)...)};
    }
};

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, const Closure<F>& f) noexcept(
    noexcept(f(std::forward<T>(v))))
    requires requires { f(std::forward<T>(v)); }
{
    return f(std::forward<T>(v));
}

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, Closure<F>& f) noexcept(noexcept(f(std::forward<T>(v))))
    requires requires { f(std::forward<T>(v)); }
{
    return f(std::forward<T>(v));
}

template<typename T, typename F>
constexpr decltype(auto) operator|(T&& v, Closure<F>&& f) noexcept(
    noexcept(std::move(f)(std::forward<T>(v))))
    requires requires { std::move(f)(std::forward<T>(v)); }
{
    return std::move(f)(std::forward<T>(v));
}

} // namespace nx
