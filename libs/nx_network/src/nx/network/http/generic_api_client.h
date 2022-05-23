// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <future>
#include <optional>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/url.h>
#include <nx/utils/url_query.h>

#include "fusion_data_http_client.h"

namespace nx::network::http {

struct DefaultApiResultCodeDescriptor
{
    using ResultCode = nx::network::http::StatusCode::Value;
};

template <class T>
concept HasResultCodeT = requires
{
    typename T::ResultCode;
};

/**
 * Base class for client of some API. E.g., REST API based on HTTP/json.
 * ApiResultCodeDescriptor MUST define:
 * - ResultCode type.
 * - If ResultCode is different from nx::network::http::StatusCode, then following method MUST be
 * defined too:
 * <pre><code>
 * public:
 *     template<typename... Output>
 *     static auto getResultCode(
 *         SystemError::ErrorCode systemErrorCode,
 *         const network::http::Response* response,
 *         const network::http::ApiRequestResult& fusionRequestResult,
 *         const Output&... output);
 * </code></pre>
 */
template<HasResultCodeT ApiResultCodeDescriptor = DefaultApiResultCodeDescriptor,
    typename Base = network::aio::BasicPollable>
class GenericApiClient: public Base
{
    using base_type = network::aio::BasicPollable;
    using ResultType = typename ApiResultCodeDescriptor::ResultCode;

public:
    GenericApiClient(const utils::Url& baseApiUrl, ssl::AdapterFunc adapterFunc);
    ~GenericApiClient();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    void setRequestTimeout(std::optional<std::chrono::milliseconds> timeout);
    void setRetryPolicy(unsigned numOfRetries, ResultType res);
    void setRetryPolicy(
        unsigned numOfRetries, nx::utils::MoveOnlyFunc<bool(ResultType)> requestSuccessed);

protected:
    virtual void stopWhileInAioThread() override;

    /**
     * @param args The last argument of this pack MUST be the completion handler which will be
     * invoked as completionHandler(ResultCode, OutputData).
     */
    template<typename OutputData, typename... InputArgsAndCompletionHandler>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        InputArgsAndCompletionHandler&&... args);

    template<typename OutputData, typename... InputArgsAndCompletionHandler>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        network::http::Credentials credentials,
        InputArgsAndCompletionHandler&&... args);

    template<
        typename OutputData,
        typename... InputArgsAndCompletionHandler,
        typename = std::enable_if_t<!std::is_same_v<
            nx::utils::tuple_first_element_t<
                std::tuple<std::decay_t<InputArgsAndCompletionHandler>...>>,
            nx::utils::UrlQuery>>>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        InputArgsAndCompletionHandler&&... args);

    template<typename Output, typename InputTuple, typename Handler>
    void makeAsyncCallWithRetries(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        network::http::Credentials credentials,
        InputTuple&& argsTuple,
        unsigned attemptNum,
        Handler handler);

    /**
     * @return std::tuple<ResultCode, Output>
     */
    template<typename Output, typename... InputArgs>
    auto makeSyncCall(InputArgs&&... inputArgs);

    const utils::Url& baseApiUrl() const;

private:
    struct Context
    {
        std::unique_ptr<network::aio::BasicPollable> client;
    };

    const utils::Url m_baseApiUrl;
    ssl::AdapterFunc m_adapterFunc;
    std::map<network::aio::BasicPollable*, Context> m_activeRequests;
    nx::Mutex m_mutex;
    std::optional<std::chrono::milliseconds> m_requestTimeout;
    unsigned m_numRetries;
    std::optional<nx::utils::MoveOnlyFunc<bool(ResultType)>> m_isRequestSucceeded;

    template<typename Output, typename... Args>
    auto createHttpClient(
        const nx::utils::Url& url,
        network::http::Credentials credentials,
        Args&&... args);

    template<typename Request, typename CompletionHandler, typename... Output>
    void processResponse(
        Request* requestPtr,
        CompletionHandler handler,
        SystemError::ErrorCode error,
        const network::http::Response* response,
        Output... output);

    Context takeContextOfRequest(nx::network::aio::BasicPollable* httpClientPtr);

    template<typename... Output>
    auto getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const network::http::ApiRequestResult& fusionRequestResult,
        const Output&... output) const;

    template<typename Output, typename ResultTuple, typename... InputArgs>
    auto makeSyncCallInternal(InputArgs&&... inputArgs);
};

//-------------------------------------------------------------------------------------------------

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
GenericApiClient<ApiResultCodeDescriptor, Base>::GenericApiClient(
    const utils::Url& baseApiUrl, ssl::AdapterFunc adapterFunc):
    m_baseApiUrl(baseApiUrl), m_adapterFunc(std::move(adapterFunc)), m_numRetries(1)
{
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
GenericApiClient<ApiResultCodeDescriptor, Base>::~GenericApiClient()
{
    Base::pleaseStopSync();
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& context : m_activeRequests)
        context.second.client->bindToAioThread(aioThread);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::setRequestTimeout(
    std::optional<std::chrono::milliseconds> timeout)
{
    m_requestTimeout = timeout;
}

template<HasResultCodeT ResultCode, typename Base>
void GenericApiClient<ResultCode, Base>::setRetryPolicy(unsigned numOfRetries, ResultType successCode)
{
    m_numRetries = numOfRetries;
    m_isRequestSucceeded = [successCode](ResultType result) { return result == successCode; };
}

template<HasResultCodeT ResultCode, typename Base>
void GenericApiClient<ResultCode, Base>::setRetryPolicy(
    unsigned numOfRetries, nx::utils::MoveOnlyFunc<bool(ResultType)> m_isRequstSucceded)
{
    m_numRetries = numOfRetries;
    m_isRequstSucceded = std::move(m_isRequstSucceded);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_activeRequests.clear();
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... InputArgsAndCompletionHandler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    InputArgsAndCompletionHandler&&... args)
{
    this->makeAsyncCall<Output>(
        method,
        requestPath,
        urlQuery,
        network::http::Credentials(),
        std::forward<InputArgsAndCompletionHandler>(args)...);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... InputArgsAndCompletionHandler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    network::http::Credentials credentials,
    InputArgsAndCompletionHandler&&... args)
{
    static_assert(sizeof...(args) > 0);

    auto argsTuple = nx::utils::make_tuple_from_arg_range<0, sizeof...(args) - 1>(
        std::forward<InputArgsAndCompletionHandler>(args)...);

    auto handler = std::get<0>(nx::utils::make_tuple_from_arg_range<sizeof...(args) - 1, 1>(
        std::forward<InputArgsAndCompletionHandler>(args)...));

    makeAsyncCallWithRetries<Output>(
        method,
        requestPath,
        urlQuery,
        std::move(credentials),
        std::move(argsTuple),
        1,
        std::move(handler));
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename InputTuple, typename Handler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCallWithRetries(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    network::http::Credentials credentials,
    InputTuple&& argsTuple,
    unsigned attemptNum,
    Handler handler)
{
    auto request = std::apply(
        [this, &requestPath, &urlQuery, &credentials](auto&&... inputArgs)
        {
            return this->template createHttpClient<Output>(
                network::url::Builder(m_baseApiUrl).appendPath(requestPath).setQuery(urlQuery),
                credentials,
                std::forward<decltype(inputArgs)>(inputArgs)...);
        },
        argsTuple);

    auto handlerWrapper = [this,
        argsTuple = std::forward<InputTuple>(argsTuple),
        handler = std::move(handler),
        method,
        requestPath,
        urlQuery,
        credentials = std::move(credentials),
        attemptNum](ResultType result, auto&&... outArgs) mutable
    {
        if (m_isRequestSucceeded && !(*m_isRequestSucceeded)(result) && attemptNum < m_numRetries)
        {
            return makeAsyncCallWithRetries<Output>(
                method,
                requestPath,
                urlQuery,
                std::move(credentials),
                std::move(argsTuple),
                ++attemptNum,
                std::move(handler));
        }

        handler(result, std::forward<decltype(outArgs)>(outArgs)...);
    };

    request->execute(
        method,
        [this, request, handlerWrapper = std::move(handlerWrapper)](auto&&... args) mutable
        {
            this->processResponse(request, std::move(handlerWrapper), std::move(args)...);
        });
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... InputArgsAndCompletionHandler, typename>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    InputArgsAndCompletionHandler&&... args)
{
    this->makeAsyncCall<Output>(
        method,
        requestPath,
        nx::utils::UrlQuery(),
        network::http::Credentials(),
        std::forward<InputArgsAndCompletionHandler>(args)...);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... InputArgs>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::makeSyncCall(InputArgs&&... inputArgs)
{
    if constexpr (std::is_same<Output, void>::value)
    {
        return makeSyncCallInternal<void, std::tuple<ResultType>>(
            std::forward<InputArgs>(inputArgs)...);
    }
    else
    {
        return makeSyncCallInternal<Output, std::tuple<ResultType, Output>>(
            std::forward<InputArgs>(inputArgs)...);
    }
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
const utils::Url& GenericApiClient<ApiResultCodeDescriptor, Base>::baseApiUrl() const
{
    return m_baseApiUrl;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... Args>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::createHttpClient(
    const nx::utils::Url& url, network::http::Credentials credentials, Args&&... args)
{
    using InputParamType = nx::utils::tuple_first_element_t<std::tuple<std::decay_t<Args>...>>;

    auto httpClient =
        std::make_unique<network::http::FusionDataHttpClient<InputParamType, Output>>(
            url, std::move(credentials), m_adapterFunc, std::forward<Args>(args)...);
    httpClient->bindToAioThread(Base::getAioThread());

    if (m_requestTimeout)
        httpClient->setRequestTimeout(*m_requestTimeout);

    auto httpClientPtr = httpClient.get();
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_activeRequests.emplace(httpClientPtr, Context{std::move(httpClient)});
    }

    return httpClientPtr;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Request, typename CompletionHandler, typename... Output>
void GenericApiClient<ApiResultCodeDescriptor, Base>::processResponse(
    Request* requestPtr,
    CompletionHandler handler,
    SystemError::ErrorCode error,
    const network::http::Response* response,
    Output... output)
{
    auto ctx = takeContextOfRequest(requestPtr);

    const auto resultCode =
        getResultCode(error, response, requestPtr->lastFusionRequestResult(), output...);

    handler(resultCode, std::move(output)...);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
typename GenericApiClient<ApiResultCodeDescriptor, Base>::Context
    GenericApiClient<ApiResultCodeDescriptor, Base>::takeContextOfRequest(
        nx::network::aio::BasicPollable* httpClientPtr)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto clientIter = m_activeRequests.find(httpClientPtr);
    NX_CRITICAL(clientIter != m_activeRequests.end());
    auto context = std::move(clientIter->second);
    m_activeRequests.erase(clientIter);

    return context;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename... Output>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::getResultCode(
    [[maybe_unused]] SystemError::ErrorCode systemErrorCode,
    const network::http::Response* response,
    const network::http::ApiRequestResult& fusionRequestResult,
    const Output&... output) const
{
    if constexpr (std::is_same<ResultType, network::http::StatusCode::Value>::value)
    {
        return response
            ? static_cast<network::http::StatusCode::Value>(response->statusLine.statusCode)
            : network::http::StatusCode::internalServerError;
    }
    else
    {
        return ApiResultCodeDescriptor::getResultCode(
            systemErrorCode, response, fusionRequestResult, output...);
    }
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename ResultTuple, typename... InputArgs>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::makeSyncCallInternal(InputArgs&&... inputArgs)
{
    std::promise<ResultTuple> done;

    makeAsyncCall<Output>(std::forward<InputArgs>(inputArgs)..., [this, &done](auto&&... args) {
        auto result = std::make_tuple(std::move(args)...);
        // Doing post to make sure all network operations are completed before returning.
        Base::post([&done, result = std::move(result)]() { done.set_value(std::move(result)); });
    });

    auto result = done.get_future().get();
    return result;
}

} // namespace nx::network::http
