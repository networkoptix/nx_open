// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ranges>

#include <nx/utils/general_macros.h>

namespace nx::actions {

template<typename Comp, typename Proj>
struct UniqueClosure;

template<typename Comp = std::ranges::equal_to, typename Proj = std::identity>
    requires(!std::ranges::range<Comp>)
constexpr UniqueClosure<Comp, Proj> unique(Comp comp = {}, Proj proj = {})
{
    return {std::move(comp), std::move(proj)};
}

// Shrinks a container if it has erase(), otherwise returns a trimmed subrange.
template<
    std::ranges::forward_range R,
    typename Proj = std::identity,
    std::indirect_equivalence_relation<std::projected<std::ranges::iterator_t<R>, Proj>> Comp =
        std::ranges::equal_to>
    requires std::permutable<std::ranges::iterator_t<R>> && (!std::is_lvalue_reference_v<R>)
constexpr auto unique(R&& input, Comp comp = {}, Proj proj = {})
{
    auto tail = std::ranges::unique(input, std::move(comp), std::move(proj));

    if constexpr (requires { input.erase(tail.begin(), tail.end()); })
    {
        input.erase(tail.begin(), tail.end());
        return input;
    }
    else
    {
        return std::ranges::subrange{
            std::make_move_iterator(std::ranges::begin(input)),
            std::make_move_iterator(tail.begin())};
    }
}

template<typename Comp, typename Proj>
struct UniqueClosure
{
    NX_NO_UNIQUE_ADDRESS Comp comp;
    NX_NO_UNIQUE_ADDRESS Proj proj;

    template<typename R>
    friend constexpr decltype(auto) operator|(R&& input, const UniqueClosure& self)
        requires requires { nx::actions::unique(std::forward<R>(input), self.comp, self.proj); }
    {
        return nx::actions::unique(std::forward<R>(input), self.comp, self.proj);
    }
};

} // namespace nx::actions
