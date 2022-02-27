// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
  * This file is for c++ utilities that are absent in modern c++ standards but can be easily
  * implemented using c++14. Some of them are in std::experimental, some present on selected
  * compilers, some are waiting in c++20 and further standards.
 */

namespace nx::utils {

/**
 * Containers like vector of non-copyble types cannot be initialized by the initializer list (it
 * requires a copy constructor), but can be initialized by the following method:
 * ```
 *     auto vector = make_container<std::vector<std::unique_ptr<Data>>>(
 *         std::make_unique<Data>(1), std::make_unique<Data>(2), std::make_unique<Data>(2));
 * ```
 */
template<typename Container, typename... Args>
Container make_container(Args... args)
{
    Container container;
    (container.push_back(std::forward<Args>(args)), ...);
    return container;
}

}   //namespace nx::utils
