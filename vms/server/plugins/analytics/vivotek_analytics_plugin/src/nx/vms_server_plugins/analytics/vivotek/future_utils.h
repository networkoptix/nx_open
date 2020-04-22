#pragma once

#include <string>
#include <unordered_set>
#include <optional>

#include <nx/utils/thread/cf/cfuture.h>

namespace nx::vms_server_plugins::analytics::vivotek {

template <typename Type = cf::unit, typename Fn>
auto initiateFuture(Fn&& fn)
{
    return cf::make_ready_future(cf::unit()).then(
        [fn = std::forward<Fn>(fn)](auto parent) mutable
        {
            parent.get();
            cf::promise<Type> promise;
            auto future = promise.get_future();
            std::move(fn)(std::move(promise));
            return future;
        });
}

template <typename Type = cf::unit, typename Executor, typename Fn>
auto initiateFuture(Executor& executor, Fn&& fn)
{
    return cf::make_ready_future(cf::unit()).then(executor,
        [fn = std::forward<Fn>(fn)](auto parent) mutable
        {
            parent.get();
            cf::promise<Type> promise;
            auto future = promise.get_future();
            std::move(fn)(std::move(promise));
            return future;
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
