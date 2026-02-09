// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

/**@file
 * This file is for c++ utilities that are absent in modern c++ standards but can be easily
 * implemented using the available c++ standard.
 */

namespace nx {

template<typename T>
std::future<std::remove_cvref_t<T>> makeReadyFuture(T&& value)
{
    std::promise<std::remove_cvref_t<T>> p;
    p.set_value(std::forward<T>(value));
    return p.get_future();
}

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

/**
 * std::vector of non-copyable types cannot be initialized by the initializer list (it
 * requires a copy constructor), but can be initialized by the following method:
 * ```
 *     auto vector = make_vector<std::unique_ptr<Data>>(1, 2, 2);
 * ```
 */
template<typename T, template<typename> typename Allocator = std::allocator, typename... Args>
auto make_vector(Args&&... args)
{
    std::vector<T, Allocator<T>> result;
    result.reserve(sizeof...(Args));
    (result.emplace_back(std::forward<Args>(args)), ...);
    return result;
}

/**
 * Converts unique_ptr of one type to another using static_cast.
 * Consumes original unique_ptr. Actually moves memory to a new unique_ptr.
 */
template<typename ResultType, typename InitialType, typename DeleterType>
std::unique_ptr<ResultType, DeleterType>
    static_unique_ptr_cast(std::unique_ptr<InitialType, DeleterType>&& sourcePtr)
{
    return std::unique_ptr<ResultType, DeleterType>(
        static_cast<ResultType>(sourcePtr.release()),
        sourcePtr.get_deleter());
}

template<typename ResultType, typename InitialType>
std::unique_ptr<ResultType> static_unique_ptr_cast(std::unique_ptr<InitialType>&& sourcePtr)
{
    return std::unique_ptr<ResultType>(
        static_cast<ResultType*>(sourcePtr.release()));
}

template<typename Object, typename Deleter>
std::unique_ptr<Object, Deleter> wrapUnique(Object* ptr, Deleter deleter)
{
    return std::unique_ptr<Object, Deleter>(ptr, std::move(deleter));
}

}   //namespace nx::utils
