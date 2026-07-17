// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <algorithm>
#include <numeric>
#include <ranges>
#include <utility>

#include <nx/ranges/to.h>
#include <nx/ranges/views/cartesian_product.h>

/*
Drop-in implementations for C++ standard library facilities that are declared in std, but not
implemented (or not fully implemented) by one or more compilers/standard libraries used in CI.

This header provides nx:: drop-ins with the same API/semantics as the corresponding std:: entities.

Once all CI toolchains provide working std:: implementations, remove the nx:: drop-ins and rename
all usages in the project from nx::... to std::....

It is illegal (undefined behavior) to declare new entities in namespace std, except for explicit
customizations permitted by the standard.
*/

namespace nx {
namespace ranges {

#if defined __cpp_lib_ranges_to_container
    using std::ranges::to;
#else
    using detail::to;
#endif

#if defined __cpp_lib_ranges_fold
    using std::ranges::fold_left;
#else
    template<std::ranges::forward_range R, typename Accum, typename BinaryOp>
    constexpr auto fold_left(R&& r, Accum&& init, BinaryOp f)
    {
        auto c = std::views::common(std::forward<R>(r));
        return std::accumulate(
            std::ranges::begin(c),
            std::ranges::end(c),
            std::forward<Accum>(init),
            std::ref(f));
    }
#endif

} // namespace ranges

namespace views {

#if defined __cpp_lib_ranges_cartesian_product
    using std::views::cartesian_product;
#else
    constexpr auto cartesian_product()
    {
        return detail::cartesianProduct();
    }

    template<typename... Ranges>
    constexpr auto cartesian_product(Ranges&&... ranges)
    {
        return detail::cartesianProduct(std::forward<Ranges>(ranges)...);
    }
#endif

} // namespace views
} // namespace nx
