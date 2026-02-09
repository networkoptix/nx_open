// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/concepts/ranges.h>
#include <nx/ranges/std_compat.h>

namespace nx::actions {

namespace detail {

template<typename C, typename T>
constexpr void appendOne(C* c, T&& v)
    requires nx::ranges::CanAppendOne<C, T&&>
{
    auto& containerRef = *c;

    if constexpr (nx::ranges::CanEmplaceBack<C, T&&>)
        containerRef.emplace_back(std::forward<T>(v));
    else if constexpr (nx::ranges::CanPushBack<C, T&&>)
        containerRef.push_back(std::forward<T>(v));
    else if constexpr (nx::ranges::CanEmplace<C, T&&>)
        (void) containerRef.emplace(std::forward<T>(v));
    else
        (void) containerRef.insert(std::forward<T>(v));
}

} // namespace detail

template<typename... Elements>
struct AppendClosure
{
    std::tuple<Elements...> elements;

    // Const closure: must copy from stored elements (uses `Elements const&`).
    // Only enabled when the container can accept elements from `const&`.
    template<typename C>
    constexpr std::remove_reference_t<C> operator()(C&& c) const&
        requires(!std::is_lvalue_reference_v<C>)
        && (nx::ranges::CanAppendOne<std::remove_reference_t<C>, Elements const&> && ...)
    {
        using R = std::remove_reference_t<C>;
        auto* containerPtr = &c;

        std::apply(
            [&](auto const&... xs)
            {
                (detail::appendOne(containerPtr, xs), ...);
            },
            elements);

        return static_cast<R&&>(c);
    }

    // Rvalue closure: may move from stored elements (uses `Elements&&`).
    template<typename C>
    constexpr std::remove_reference_t<C> operator()(C&& c) &&
        requires(!std::is_lvalue_reference_v<C>)
        && (nx::ranges::CanAppendOne<std::remove_reference_t<C>, Elements &&> && ...)
    {
        using R = std::remove_reference_t<C>;
        auto* containerPtr = &c;

        std::apply(
            [&](auto&&... xs)
            {
                (detail::appendOne(containerPtr, std::forward<decltype(xs)>(xs)), ...);
            },
            std::move(elements));

        return static_cast<R&&>(c);
    }

    // RHS convention: prefer `const AppendClosure&`.
    template<typename C>
    friend constexpr std::remove_reference_t<C> operator|(C&& c, AppendClosure const& f)
        requires(!std::is_lvalue_reference_v<C>) && requires { f(std::forward<C>(c)); }
    {
        return f(std::forward<C>(c));
    }

    template<typename C>
    friend constexpr std::remove_reference_t<C> operator|(C&& c, AppendClosure&& f)
        requires(!std::is_lvalue_reference_v<C>) && requires { std::move(f)(std::forward<C>(c)); }
    {
        return std::move(f)(std::forward<C>(c));
    }
};

/*
append(container, xs...) -> container

Takes ownership of the container (rejects lvalues). Appends one or more elements and returns the
container back to enable pipelines.

Usage:
  auto out = append(std::vector<int>{}, 1, 2, 3);

  auto out = makeVec()
      | append(4)
      | append(8, 15);

Move/copy explicitly:
  auto out = append(std::vector<int>(vec), 1);  // copy
  auto out = append(std::move(vec), 1);         // move
*/
template<typename C, typename... Elements>
constexpr std::remove_reference_t<C> append(C&& c, Elements&&... xs)
    requires(!std::is_lvalue_reference_v<C>) && (sizeof...(Elements) > 0)
    && (nx::ranges::CanAppendOne<std::remove_reference_t<C>, Elements &&> && ...)
{
    (detail::appendOne(&c, std::forward<Elements>(xs)), ...);
    return std::forward<C>(c);
}

/*
append(xs...) -> AppendClosure<...>

Factory overload returning a pipeable closure that appends stored elements to an owning container.

Usage:
  auto out = makeVec() | append(4) | append(8, 15);

Notes:
- Rejects the common mistake `append(container_lvalue, x, ...)` by disabling when the first
argument is a range and there is at least one more argument.
*/
template<typename First, typename... Rest>
    requires((sizeof...(Rest) == 0) || !std::ranges::range<std::remove_cvref_t<First>>)
constexpr AppendClosure<std::decay_t<First>, std::decay_t<Rest>...> append(
    First&& first,
    Rest&&... rest)
{
    return {
        std::tuple<std::decay_t<First>, std::decay_t<Rest>...>(
            std::forward<First>(first),
            std::forward<Rest>(rest)...),
    };
}

} // namespace nx::actions
