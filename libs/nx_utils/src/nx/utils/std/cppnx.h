// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
  * This file is for c++ utilities that are absent in modern c++ standards but can be easily
  * implemented using c++14. Some of them are in std::experimental, some present on selected
  * compilers, some are waiting in c++20 and further standards.
 */

namespace nx::utils {

/**
 * std::vector of non-copyable types cannot be initialized by the initializer list (it
 * requires a copy constructor), but can be initialized by the following method:
 * ```
 *     auto vector = make_vector(std::make_unique<Data>(1),
 *         std::make_unique<Data>(2), std::make_unique<Data>(2));
 * ```
 */
template<template<typename> typename Allocator = std::allocator, typename... Elements>
auto make_vector(Elements&&... e)
{
    using ValueType = std::common_type_t<std::decay_t<Elements>...>;
    std::vector<ValueType, Allocator<ValueType>> result;
    result.reserve(sizeof...(Elements));
    (result.emplace_back(std::forward<Elements>(e)), ...);
    return result;
}

#if !defined(__cpp_lib_bind_back)
    template<class F, class... BoundArgs>
    constexpr auto bind_back(F&& f, BoundArgs&&... boundArgs)
    {
        return
            [f = std::forward<F>(f), ... boundArgs = std::forward<BoundArgs>(boundArgs)]
                <typename... Args>(Args&&... args) mutable -> decltype(auto)
            {
                return std::invoke(f,
                    std::forward<Args>(args)...,
                    // FIXME: #skolesnik Investigate why this breaks compilation
                    // std::forward<BoundArgs>(boundArgs)...
                    boundArgs...);
            };
    }
#else
    using std::bind_back;
#endif

}   //namespace nx::utils
