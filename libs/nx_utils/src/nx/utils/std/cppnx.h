// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>
#include <utility>
#include <vector>

/**@file
 * This file is for c++ utilities that are absent in modern c++ standards but can be easily
 * implemented using the available c++ standard.
 */

namespace nx {

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

}   //namespace nx::utils
