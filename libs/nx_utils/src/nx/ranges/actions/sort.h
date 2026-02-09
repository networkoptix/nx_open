// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

#include <nx/utils/general_macros.h>

namespace nx::actions {

template<typename Comp, typename Proj>
struct SortClosure;

// Sorts an rvalue range in-place and returns it.
template<
    std::ranges::random_access_range R,
    typename Proj = std::identity,
    std::indirect_strict_weak_order<std::projected<std::ranges::iterator_t<R>, Proj>> Comp =
        std::ranges::less>
    requires std::sortable<std::ranges::iterator_t<R>, Comp, Proj>
    && (!std::is_lvalue_reference_v<R>)
constexpr auto sort(R&& input, Comp comp = {}, Proj proj = {})
{
    std::ranges::sort(input, std::move(comp), std::move(proj));
    return input;
}

template<typename Comp, typename Proj>
struct SortClosure
{
    NX_NO_UNIQUE_ADDRESS Comp comp;
    NX_NO_UNIQUE_ADDRESS Proj proj;

    template<typename R>
    friend constexpr decltype(auto) operator|(R&& input, const SortClosure& self)
        requires requires { nx::actions::sort(std::forward<R>(input), self.comp, self.proj); }
    {
        return nx::actions::sort(std::forward<R>(input), self.comp, self.proj);
    }
};

template<typename Comp = std::ranges::less, typename Proj = std::identity>
    requires(!std::ranges::range<Comp>)
constexpr SortClosure<Comp, Proj> sort(Comp comp = {}, Proj proj = {})
{
    return {std::move(comp), std::move(proj)};
}

} // namespace nx::actions
