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
    // TODO: #akolesnikov Rename to type.
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
 *
 * Note: Supports concurrent requests.
 */
template<
    HasResultCodeT ApiResultCodeDescriptor = DefaultApiResultCodeDescriptor,
    typename Base = network::aio::BasicPollable
>
class GenericApiClient:
    public Base
{
    using base_type = Base;
    using ResultType = typename ApiResultCodeDescriptor::ResultCode;

public:
    GenericApiClient(const utils::Url& baseApiUrl, ssl::AdapterFunc adapterFunc);
    ~GenericApiClient();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    // Deprecated. Use httpClientOptions().setTimeouts() instead.
    void setRequestTimeout(std::optional<std::chrono::milliseconds> timeout);

    void setRetryPolicy(unsigned numOfRetries, ResultType res);
    void setRetryPolicy(
        unsigned numOfRetries,
        nx::utils::MoveOnlyFunc<bool(ResultType)> requestSuccessed);

    // Deprecated. Use httpClientOptions().setCredentials() instead.
    void setHttpCredentials(Credentials credentials);
    std::optional<Credentials> getHttpCredentials() const;

    /**
     * Full set of HTTP client options that are set on every request.
     */
    ClientOptions& httpClientOptions() { return this->m_clientOptions; }
    const ClientOptions& httpClientOptions() const { return this->m_clientOptions; }

    /**
     * If enabled, then HTTP response body is cache in memory along with the ETag header value
     * from the response. If the same request is made again and there is a cached value, then the
     * "If-None-Match: <cached ETag value>" is added to the request and, if the server responds
     * with "304 Not Modified" status, then the cached value is returned.
     * The functionality is useful when there are multiple requests (e.g., periodic ones) with
     * rarely changing reply.
     * Disabled by default.
     */
    void setCacheEnabled(bool enabled) { this->m_cacheEnabled = enabled; }

protected:
    /**
     * @param ResponseFetchers 0 or more fetchers applied to the HTTP response object.
     * Returned result is passed to the handler.
     * @param handler. Signature: void(ResultType, OutputData, ResponseFetchers()(response)...).
     */
    template<typename OutputData, typename... ResponseFetchers, typename InputData, typename Handler>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        InputData&& inputData,
        Handler handler);

    // Same as the previous overload, but without input data.
    template<typename OutputData, typename... ResponseFetchers, typename Handler>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        Handler handler);

    template<typename Output, typename InputTuple, typename Handler,
        typename SerializationLibWrapper = detail::NxReflectWrapper, typename... ResponseFetchers>
    void makeAsyncCallWithRetries(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::utils::UrlQuery& urlQuery,
        std::tuple<ResponseFetchers...> /*dummy*/,
        InputTuple&& argsTuple,
        unsigned attemptNum,
        Handler handler);

    /**
     * @return std::tuple<ResultCode, Output>
     */
    template<typename Output, typename... InputArgs>
    auto makeSyncCall(InputArgs&&... inputArgs);

protected:
    virtual void stopWhileInAioThread() override;

    const utils::Url& baseApiUrl() const;

private:
    struct Context
    {
        std::unique_ptr<network::aio::BasicPollable> client;
    };

    struct BaseCacheEntry
    {
        std::string etag;
        ResultType result{};

        virtual ~BaseCacheEntry() = default;
    };

    template<typename T>
    struct CacheEntry: BaseCacheEntry
    {
        T reply;
    };

    template<>
    struct CacheEntry<void>: BaseCacheEntry
    {
    };

    const utils::Url m_baseApiUrl;
    ssl::AdapterFunc m_adapterFunc;
    std::map<network::aio::BasicPollable*, Context> m_activeRequests;
    nx::Mutex m_mutex;
    std::optional<std::chrono::milliseconds> m_requestTimeout;
    unsigned int m_numRetries = 1;
    std::optional<nx::utils::MoveOnlyFunc<bool(ResultType)>> m_isRequestSucceeded;
    ClientOptions m_clientOptions;
    bool m_cacheEnabled = false;
    // Actual value has type CacheEntry<T> where T is passed via OutputData template parameter.
    std::unordered_map<std::string /*GET <path>*/, std::shared_ptr<const BaseCacheEntry>> m_cache;

    template<typename Output, typename SerializationLibWrapper, typename... Args>
    auto createHttpClient(const nx::utils::Url& url, Args&&... args);

    std::string makeCacheKey(const network::http::Method& method, const std::string& requestPath);

    template<typename CachedType>
    std::shared_ptr<const CacheEntry<CachedType>> findCacheEntry(
        const network::http::Method& method,
        const std::string& requestPath);

    template<typename CachedType>
    void saveToCache(
        const network::http::Method& method,
        const std::string& requestPath,
        CacheEntry<CachedType>&& cachedType);

    template<typename... ResponseFetchers, typename Request, typename CompletionHandler,
        typename CachedType, typename... Output
    >
    void processResponse(
        const network::http::Method& method,
        const std::string& requestPath,
        Request* requestPtr,
        std::shared_ptr<const CacheEntry<CachedType>>,
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

/**
 * Introduced as a work around a weird link error on mswin.
 * TODO: akolesnikov Review it again. It has something to do with GenericApiClient inheriting
 * exported class aio::BasicPollable.
 */
class NX_NETWORK_API DefaultGenericApiClient:
    public GenericApiClient<>
{
    using base_type = GenericApiClient<>;

public:
    using base_type::base_type;

    using base_type::makeAsyncCall;
    using base_type::makeSyncCall;
};

//-------------------------------------------------------------------------------------------------

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
GenericApiClient<ApiResultCodeDescriptor, Base>::GenericApiClient(
    const utils::Url& baseApiUrl,
    ssl::AdapterFunc adapterFunc)
    :
    m_baseApiUrl(baseApiUrl),
    m_adapterFunc(std::move(adapterFunc))
{
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
GenericApiClient<ApiResultCodeDescriptor, Base>::~GenericApiClient()
{
    this->pleaseStopSync();
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

template<HasResultCodeT ResultCode, typename Base>
void GenericApiClient<ResultCode, Base>::setHttpCredentials(Credentials credentials)
{
    m_clientOptions.setCredentials(std::move(credentials));
}

template<HasResultCodeT ResultCode, typename Base>
std::optional<Credentials> GenericApiClient<ResultCode, Base>::getHttpCredentials() const
{
    return !m_clientOptions.credentials().username.empty()
        ? std::make_optional(m_clientOptions.credentials()) : std::nullopt;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_activeRequests.clear();
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... ResponseFetchers, typename InputData, typename Handler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    InputData&& inputData,
    Handler handler)
{
    makeAsyncCallWithRetries<Output>(
        method,
        requestPath,
        urlQuery,
        std::tuple<ResponseFetchers...>{},
        std::make_tuple(std::forward<InputData>(inputData)),
        1,
        std::move(handler));
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... ResponseFetchers, typename Handler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    Handler handler)
{
    makeAsyncCallWithRetries<Output>(
        method,
        requestPath,
        urlQuery,
        std::tuple<ResponseFetchers...>{},
        std::tuple<>{},
        1,
        std::move(handler));
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output,
    typename InputTuple, typename Handler, typename SerializationLibWrapper, typename... ResponseFetchers
>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCallWithRetries(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::utils::UrlQuery& urlQuery,
    std::tuple<ResponseFetchers...> /*dummy*/,
    InputTuple&& argsTuple,
    unsigned attemptNum,
    Handler handler)
{
    auto request = std::apply(
        [this, &requestPath, &urlQuery](auto&&... inputArgs)
        {
            return this->template createHttpClient<Output, SerializationLibWrapper>(
                network::url::Builder(m_baseApiUrl).appendPath(requestPath).setQuery(urlQuery),
                std::forward<decltype(inputArgs)>(inputArgs)...);
        },
        argsTuple);

    auto handlerWrapper = [this,
        argsTuple = std::forward<InputTuple>(argsTuple),
        handler = std::move(handler),
        method,
        requestPath,
        urlQuery,
        attemptNum](ResultType result, auto&&... outArgs) mutable
    {
        if (m_isRequestSucceeded && !(*m_isRequestSucceeded)(result) && attemptNum < m_numRetries)
        {
            return makeAsyncCallWithRetries<Output>(
                method,
                requestPath,
                urlQuery,
                std::tuple<ResponseFetchers...>{},
                std::move(argsTuple),
                ++attemptNum,
                std::move(handler));
        }

        handler(result, std::forward<decltype(outArgs)>(outArgs)...);
    };

    auto cacheEntry = findCacheEntry<Output>(method, requestPath);
    if (cacheEntry)
        request->httpClient().addAdditionalHeader("If-None-Match", cacheEntry->etag);

    request->execute(
        method,
        [this, method, requestPath, request, cacheEntry, handlerWrapper = std::move(handlerWrapper)](
            auto&&... args) mutable
        {
            this->processResponse<ResponseFetchers...>(
                method,
                requestPath,
                request,
                std::move(cacheEntry),
                std::move(handlerWrapper),
                std::forward<decltype(args)>(args)...);
        });
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
template<typename Output, typename SerializationLibWrapper, typename... Args>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::createHttpClient(
    const nx::utils::Url& url,
    Args&&... args)
{
    using InputParamType = nx::utils::tuple_first_element_t<std::tuple<std::decay_t<Args>...>>;

    auto httpClient = std::make_unique<
        network::http::FusionDataHttpClient<InputParamType, Output, SerializationLibWrapper>>(
            url, http::Credentials(), m_adapterFunc, std::forward<Args>(args)...);
    httpClient->bindToAioThread(this->getAioThread());

    httpClient->httpClient().assignOptions(m_clientOptions);

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
std::string GenericApiClient<ApiResultCodeDescriptor, Base>::makeCacheKey(
    const network::http::Method& method,
    const std::string& requestPath)
{
    return nx::utils::buildString(method.toString(), " ", requestPath);
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename CachedType>
std::shared_ptr<const typename GenericApiClient<ApiResultCodeDescriptor, Base>::template CacheEntry<CachedType>>
    GenericApiClient<ApiResultCodeDescriptor, Base>::findCacheEntry(
        const network::http::Method& method,
        const std::string& requestPath)
{
    if (!m_cacheEnabled)
        return nullptr;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        auto cacheIter = m_cache.find(makeCacheKey(method, requestPath));
        if (cacheIter != m_cache.end())
            return std::dynamic_pointer_cast<const CacheEntry<CachedType>>(cacheIter->second);
    }

    return nullptr;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename CachedType>
void GenericApiClient<ApiResultCodeDescriptor, Base>::saveToCache(
    const network::http::Method& method,
    const std::string& requestPath,
    CacheEntry<CachedType>&& cachedType)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_cache[makeCacheKey(method, requestPath)] =
        std::make_shared<CacheEntry<CachedType>>(std::forward<CacheEntry<CachedType>>(cachedType));
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename... ResponseFetchers, typename Request, typename CompletionHandler,
    typename CachedType, typename... Output
>
void GenericApiClient<ApiResultCodeDescriptor, Base>::processResponse(
    const network::http::Method& method,
    const std::string& requestPath,
    Request* requestPtr,
    std::shared_ptr<const CacheEntry<CachedType>> cacheEntry,
    CompletionHandler handler,
    SystemError::ErrorCode error,
    const network::http::Response* response,
    Output... output)
{
    auto ctx = takeContextOfRequest(requestPtr);

    const auto resultCode =
        getResultCode(error, response, requestPtr->lastFusionRequestResult(), output...);

    if constexpr (!std::is_void_v<CachedType>)
    {
        if (cacheEntry && response && response->statusLine.statusCode == StatusCode::notModified)
        {
            handler(cacheEntry->result, cacheEntry->reply, ResponseFetchers()(*response)...);
            return;
        }

        if (m_cacheEnabled &&
            response &&
            response->statusLine.statusCode == StatusCode::ok &&
            response->headers.contains("ETag"))
        {
            CacheEntry<CachedType> entry;
            entry.etag = getHeaderValue(response->headers, "ETag");
            entry.result = resultCode;
            entry.reply = (output, ...);
            saveToCache(method, requestPath, std::move(entry));
        }
    }

    handler(resultCode, std::move(output)...,
        ResponseFetchers()(response ? *response : network::http::Response())...);
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
        if (systemErrorCode != SystemError::noError)
            return network::http::StatusCode::internalServerError;

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

    makeAsyncCall<Output>(
        std::forward<InputArgs>(inputArgs)...,
        [this, &done](auto&&... args) {
            auto result = std::make_tuple(std::move(args)...);
            // Doing post to make sure all network operations are completed before returning.
            this->post([&done, result = std::move(result)]() { done.set_value(std::move(result)); });
        });

    auto result = done.get_future().get();
    return result;
}

} // namespace nx::network::http
