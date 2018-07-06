#pragma once

#include <functional>

#include <nx/utils/std/future.h>

/**
 * Calls asynchronous method that accepts completion handler as std::function<ResultType>
 * and waits for completion.
 */
template<typename ResultType>
std::tuple<ResultType> makeSyncCall(
    std::function<void(std::function<void(ResultType)>)> function)
{
    nx::utils::promise<ResultType> promise;
    auto future = promise.get_future();
    function([&promise](ResultType resCode) { promise.set_value(resCode); });
    return std::make_tuple(future.get());
}

template<typename ResultType, typename OutArg1>
std::tuple<ResultType, OutArg1> makeSyncCall(
    std::function<void(std::function<void(ResultType, OutArg1)>)> function)
{
    nx::utils::promise<ResultType> promise;
    auto future = promise.get_future();
    OutArg1 result;
    function(
        [&promise, &result](ResultType resCode, OutArg1 outArg1)
        {
            result = std::move(outArg1);
            promise.set_value(resCode);
        });
    future.wait();
    return std::make_tuple(future.get(), std::move(result));
}

template<typename ResultType, typename OutArg1, typename FuncPtr, typename Arg1, typename ... Args>
std::tuple<ResultType, OutArg1> makeSyncCall(FuncPtr funcPtr, Arg1 arg1, Args... args)
{
    return makeSyncCall<ResultType, OutArg1>(std::bind(funcPtr, arg1, args...));
}
