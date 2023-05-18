// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <tuple>

namespace nx::utils::algorithm::detail {

template<typename T>
struct accessor_traits: accessor_traits<decltype(&T::operator())>
{
};

template<typename T, typename R>
struct accessor_traits<R T::*> {
    using result = R;
    using args = std::tuple<T>;
    using arg1 = typename std::tuple_element<0, args>;
};

template<typename T, typename R, typename... Args>
struct accessor_traits<R (T::*)(Args...)>
{
    using result = R;
    using args = std::tuple<Args...>;
    using arg1 = typename std::tuple_element<0, args>;
};

template<typename T, typename R, typename... Args>
struct accessor_traits<R (T::*)(Args...) const>
{
    using result = R;
    using args = std::tuple<Args...>;
    using arg1 = typename std::tuple_element<0, args>;
};

template<typename R, typename... Args>
struct accessor_traits<R (*)(Args...)>
{
    using result = R;
    using args = std::tuple<Args...>;
    using arg1 = typename std::tuple_element<0, args>;
};

template<typename R, typename... Args>
struct accessor_traits<std::function<R(Args...)>>
{
    using result = R;
    using args = std::tuple<Args...>;
    using arg1 = typename std::tuple_element<0, args>;
};

template<typename R, typename... Args>
struct accessor_traits<R(Args...)>
{
    using result = R;
    using args = std::tuple<Args...>;
    using arg1 = typename std::tuple_element<0, args>;
};

} // namespace nx::utils::algorithm::detail
