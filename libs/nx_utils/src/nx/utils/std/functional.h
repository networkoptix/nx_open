// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace nx::utils {

#if defined __cpp_lib_bind_back
    using std::bind_back;
#else
    template<typename F, typename... Bound>
    constexpr auto bind_back(F&& f, Bound&&... bound)
    {
        return
            [f = std::forward<F>(f), ... bound = std::forward<Bound>(bound)]<typename... Args>
                requires std::invocable<F&&, Args&&..., Bound&&...>
                (Args&&... args) mutable -> decltype(auto)
            {
                return std::invoke(std::forward<decltype(f)>(f),
                    std::forward<Args>(args)...,
                    std::forward<decltype(bound)>(bound)...);
            };
    }
#endif

#if defined __cpp_lib_bind_front
    using std::bind_front;
#else
    template<typename F, typename... Bound>
    constexpr auto bind_front(F&& f, Bound&&... bound)
    {
        return
            [f = std::forward<F>(f), ... bound = std::forward<Bound>(bound)]<typename... Args>
                requires std::invocable<F&&, Bound&&..., Args&&...>
                (Args&&... args) mutable -> decltype(auto)
            {
                return std::invoke(std::forward<decltype(f)>(f),
                    std::forward<decltype(bound)>(bound)...,
                    std::forward<Args>(args)...);
            };
    }
#endif

} // namespace nx::utils
