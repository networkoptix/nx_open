// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ranges>

#include <nx/concepts/ranges.h>
#include <nx/ranges/views/intersections.h>

namespace nx::views {

template<typename Index = std::size_t>
    requires std::integral<Index>
struct IndexedClosure
{
    Index from{};

    template<typename R>
        requires std::ranges::range<R>
        && (std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
    constexpr auto operator()(R&& r) const
    {
        return std::views::zip(
            std::invoke(
                [&]() -> decltype(auto)
                {
                    // std::views::all(r) is only well-formed for "viewable" ranges:
                    // - views (already view types),
                    // - lvalue ranges (produce ref_view),
                    // - or borrowable rvalue ranges.
                    //
                    // For non-viewable rvalue ranges (e.g., a moved std::vector),
                    // std::views::all(r) is ill-formed because it would create a dangling view.
                    // In that case we wrap the range into std::ranges::owning_view, which takes
                    // ownership and makes it a safe view to zip over.
                    if constexpr (std::ranges::viewable_range<R>)
                        return std::views::all(std::forward<R>(r));
                    else
                        return std::ranges::owning_view<std::remove_cvref_t<R>>(std::forward<R>(r));
                }),
            std::views::iota(from));
    }

    template<typename R>
        requires std::ranges::range<R>
        && (std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
    friend constexpr auto operator|(R&& range, IndexedClosure self)
    {
        return self(std::forward<R>(range));
    }
};

/*
indexed(from = 0) -> adaptor (pipeable).

Usage:
  const std::vector<int> v{10, 20, 30};
  for (const auto& [value, index] : v | indexed()) { ... }

Element decomposition:
  [value, index]
*/
template<typename Index = std::size_t>
    requires std::integral<Index>
constexpr IndexedClosure<Index> indexed(Index from = Index{})
{
    return {.from = from};
}

/*
indexed(range, from = 0) -> view.

Usage:
  const std::vector<int> v{10, 20, 30};

  for (const auto& [value, index] : indexed(v)) { ... }
  for (const auto& [value, index] : indexed(v, 100)) { ... }

Explicit Index type:
  for (const auto& [value, index] : indexed<std::uint32_t>(v, 100)) { ... }

Element decomposition:
  [value, index]
*/
template<typename Index = std::size_t, typename R>
    requires std::integral<Index> && std::ranges::range<R>
    && (std::ranges::viewable_range<R> || std::movable<std::remove_cvref_t<R>>)
constexpr auto indexed(R&& r, Index from = Index{})
{
    return IndexedClosure<Index>{.from = from}(std::forward<R>(r));
}


} // namespace nx::views
