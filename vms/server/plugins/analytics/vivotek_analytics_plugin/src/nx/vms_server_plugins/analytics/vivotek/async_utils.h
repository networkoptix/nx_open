#pragma once

#include <future>
#include <utility>
#include <exception>
#include <type_traits>

namespace nx::vms_server_plugins::analytics::vivotek {

template <typename Result = void, typename Initiator>
Result waitHandler(Initiator initiate)
{
    std::promise<Result> promise;
    auto future = promise.get_future();

    if constexpr (std::is_void_v<Result>)
        initiate(
            [promise = std::move(promise)](auto exceptionPtr) mutable
            {
                if (exceptionPtr)
                    promise.set_exception(exceptionPtr);

                promise.set_value();
            });
    else
        initiate(
            [promise = std::move(promise)](auto exceptionPtr, auto&& result) mutable
            {
                if (exceptionPtr)
                    promise.set_exception(exceptionPtr);

                try
                {
                    promise.set_value(std::forward<decltype(result)>(result));
                }
                catch (...)
                {
                    promise.set_exception(std::current_exception());
                }
            });

    return future.get();
}

} // namespace nx::vms_server_plugins::analytics::vivotek
