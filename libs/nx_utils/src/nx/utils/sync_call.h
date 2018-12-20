#pragma once

#include <functional>

#include <nx/utils/std/future.h>

namespace detail {

template<typename ResultType, typename... OutArgs>
std::tuple<ResultType, OutArgs...> makeSyncCall(
    std::function<void(std::function<void(ResultType, OutArgs...)>)> function)
{
    nx::utils::promise<ResultType> promise;
    auto future = promise.get_future();
    std::tuple<OutArgs...> resultArgs;
    function(
        [&promise, &resultArgs](ResultType resCode, OutArgs... args)
        {
            resultArgs = std::make_tuple(std::move(args)...);
            promise.set_value(resCode);
        });
    future.wait();
    ResultType resultValue = future.get();
    return std::tuple_cat(std::tie(resultValue), std::move(resultArgs));
}

} // namespace detail

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

template<typename ResultType, typename OutArg1, typename FuncPtr, typename Arg1, typename ... Args>
std::tuple<ResultType, OutArg1> makeSyncCall(FuncPtr funcPtr, Arg1 arg1, Args... args);

template<typename ResultType, typename... OutArgs, typename FuncType>
std::tuple<ResultType, OutArgs...> makeSyncCall(FuncType f)
{
    return detail::makeSyncCall<ResultType, OutArgs...>(
        std::function<void(std::function<void(ResultType, OutArgs...)>)>(std::move(f)));
}

template<typename ResultType, typename... OutArgs>
std::tuple<ResultType, OutArgs...> makeSyncCall(
    std::function<void(std::function<void(ResultType, OutArgs...)>)> function)
{
    return detail::makeSyncCall<ResultType, OutArgs...>(std::move(function));
}

template<typename ResultType, typename OutArg1, typename FuncPtr, typename Arg1, typename ... Args>
std::tuple<ResultType, OutArg1> makeSyncCall(FuncPtr funcPtr, Arg1 arg1, Args... args)
{
    return makeSyncCall<ResultType, OutArg1>(std::bind(funcPtr, arg1, args...));
}
