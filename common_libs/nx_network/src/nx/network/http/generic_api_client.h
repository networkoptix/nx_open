#pragma once

#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/type_utils.h>
#include <nx/utils/url.h>

#include "fusion_data_http_client.h"

namespace nx::network::http {

/**
 * Base class for client of REST API based on HTTP/json.
 * Implementation MUST define:
 * - ResultCode type.
 * - If ResultCode is different from nx::network::http::StatusCode, then following method MUST be 
 * defined too:
 * <pre><code>
 * template<typename Output>
 * auto getResultCode(
 *     SystemError::ErrorCode systemErrorCode,
 *     const network::http::Response* response,
 *     const Output&... output);
 * </code></pre>
 */
template<typename Implementation>
class GenericApiClient:
    public network::aio::BasicPollable
{
    using base_type = network::aio::BasicPollable;

public:
    GenericApiClient(const utils::Url& baseApiUrl);
    ~GenericApiClient();

    virtual void bindToAioThread(
        network::aio::AbstractAioThread* aioThread) override;

protected:
    virtual void stopWhileInAioThread() override;

    /**
     * @param completionHandler Invoked as completionHandler(ResultCode, OutputData).
     */
    template<typename OutputData, typename CompletionHandler, typename... InputArgs>
    void makeAsyncCall(
        const std::string& requestPath,
        CompletionHandler completionHandler,
        InputArgs... inputArgs);

    /**
     * @return std::tuple<ResultCode, Output>
     */
    template<typename Output, typename... InputArgs>
    auto makeSyncCall(
        const std::string& requestPath,
        InputArgs... inputArgs);

    const utils::Url& baseApiUrl() const;

private:
    struct Context
    {
        std::unique_ptr<network::aio::BasicPollable> client;
    };

    const utils::Url m_baseApiUrl;
    std::map<network::aio::BasicPollable*, Context> m_activeRequests;
    QnMutex m_mutex;

    template<typename Output, typename... InputArgs>
    auto createHttpClient(
        const std::string& requestPath,
        InputArgs... inputArgs);

    template<typename CompletionHandler, typename... Output>
    void processResponse(
        network::aio::BasicPollable* requestPtr,
        CompletionHandler handler,
        SystemError::ErrorCode error,
        const network::http::Response* response,
        Output... output);

    Context takeContextOfRequest(
        nx::network::aio::BasicPollable* httpClientPtr);

    template<typename... Output>
    auto getResultCode(
        SystemError::ErrorCode systemErrorCode,
        const network::http::Response* response,
        const Output&... output) const;

    template<typename Output, typename ResultTuple, typename... InputArgs>
    auto makeSyncCallInternal(
        const std::string& requestPath,
        InputArgs... inputArgs);
};

//-------------------------------------------------------------------------------------------------

template<typename Implementation>
GenericApiClient<Implementation>::GenericApiClient(const utils::Url& baseApiUrl):
    m_baseApiUrl(baseApiUrl)
{
}

template<typename Implementation>
GenericApiClient<Implementation>::~GenericApiClient()
{
    pleaseStopSync();
}

template<typename Implementation>
void GenericApiClient<Implementation>::bindToAioThread(
    network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    QnMutexLocker lock(&m_mutex);
    for (const auto& context : m_activeRequests)
        context.second.client->bindToAioThread(aioThread);
}

template<typename Implementation>
void GenericApiClient<Implementation>::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_activeRequests.clear();
}

template<typename Implementation>
template<typename Output, typename CompletionHandler, typename... InputArgs>
void GenericApiClient<Implementation>::makeAsyncCall(
    const std::string& requestPath,
    CompletionHandler handler,
    InputArgs... inputArgs)
{
    auto request = createHttpClient<Output>(
        requestPath,
        std::move(inputArgs)...);

    request->execute(
        [this, request, handler = std::move(handler)](
            auto&&... args) mutable
        {
            processResponse(request, std::move(handler), std::move(args)...);
        });
}

template<typename Implementation>
template<typename Output, typename... InputArgs>
auto GenericApiClient<Implementation>::makeSyncCall(
    const std::string& requestPath,
    InputArgs... inputArgs)
{
    if constexpr (std::is_same<Output, void>::value)
    {
        return makeSyncCallInternal<
            void,
            std::tuple<typename Implementation::ResultCode>>(
                requestPath, std::move(inputArgs)...);
    }
    else
    {
        return makeSyncCallInternal<
            Output,
            std::tuple<typename Implementation::ResultCode, Output>>(
                requestPath, std::move(inputArgs)...);
    }
}

template<typename Implementation>
const utils::Url& GenericApiClient<Implementation>::baseApiUrl() const
{
    return m_baseApiUrl;
}

template<typename Implementation>
template<typename Output, typename... InputArgs>
auto GenericApiClient<Implementation>::createHttpClient(
    const std::string& requestPath,
    InputArgs... inputArgs)
{
    using InputParamType =
        typename nx::utils::tuple_first_element<void, std::tuple<InputArgs...>>::type;

    auto httpClient = std::make_unique<
        network::http::FusionDataHttpClient<InputParamType, Output>>(
            network::url::Builder(m_baseApiUrl).appendPath(requestPath.c_str()),
            network::http::AuthInfo(),
            std::move(inputArgs)...);
    httpClient->bindToAioThread(getAioThread());

    auto httpClientPtr = httpClient.get();
    {
        QnMutexLocker lock(&m_mutex);
        m_activeRequests.emplace(httpClientPtr, Context{ std::move(httpClient) });
    }

    return httpClientPtr;
}

template<typename Implementation>
template<typename CompletionHandler, typename... Output>
void GenericApiClient<Implementation>::processResponse(
    network::aio::BasicPollable* requestPtr,
    CompletionHandler handler,
    SystemError::ErrorCode error,
    const network::http::Response* response,
    Output... output)
{
    Context context = takeContextOfRequest(requestPtr);

    const auto resultCode =
        static_cast<const Implementation*>(this)->getResultCode(
            error, response, output...);

    handler(resultCode, std::move(output)...);
}

template<typename Implementation>
typename GenericApiClient<Implementation>::Context 
    GenericApiClient<Implementation>::takeContextOfRequest(
        nx::network::aio::BasicPollable* httpClientPtr)
{
    QnMutexLocker lock(&m_mutex);
    auto clientIter = m_activeRequests.find(httpClientPtr);
    NX_CRITICAL(clientIter != m_activeRequests.end());
    auto context = std::move(clientIter->second);
    m_activeRequests.erase(clientIter);

    return context;
}

template<typename Implementation>
template<typename... Output>
auto GenericApiClient<Implementation>::getResultCode(
    [[maybe_unused]] SystemError::ErrorCode systemErrorCode,
    const network::http::Response* response,
    const Output&... output) const
{
    if constexpr (std::is_same<
        typename Implementation::ResultCode,
        network::http::StatusCode::Value>::value)
    {
        return response
            ? static_cast<network::http::StatusCode::Value>(
                response->statusLine.statusCode)
            : network::http::StatusCode::internalServerError;
    }
    else
    {
        return static_cast<const Implementation*>(this)->getResultCode(
            systemErrorCode,
            response,
            output...);
    }
}

template<typename Implementation>
template<typename Output, typename ResultTuple, typename... InputArgs>
auto GenericApiClient<Implementation>::makeSyncCallInternal(
    const std::string& requestPath,
    InputArgs... inputArgs)
{
    std::promise<ResultTuple> done;

    makeAsyncCall<Output>(
        requestPath,
        [this, &done](auto&&... args)
        {
            auto result = std::make_tuple(std::move(args)...);
            // Doing post to make sure all network operations are completed before returning.
            post(
                [&done, result = std::move(result)]()
                {
                    done.set_value(std::move(result));
                });
        },
        std::move(inputArgs)...);

    auto result = done.get_future().get();
    return result;
}

} // namespace nx::network::http
