// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <future>
#include <optional>
#include <tuple>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/datetime.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/thread/mutex.h>
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

protected:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient,
        std::function<void(network::http::AsyncClient*)>>;

public:
    GenericApiClient(
        const Url& baseApiUrl,
        ssl::AdapterFunc adapterFunc,
        int idleConnectionsLimit = 0,
        Qn::SerializationFormat sfmt = Qn::SerializationFormat::json);

    ~GenericApiClient();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    // Deprecated. Use httpClientOptions().setTimeouts() instead.
    void setRequestTimeout(std::optional<std::chrono::milliseconds> timeout);

    void setRetryPolicy(unsigned numOfRetries, ResultType res);
    void setRetryPolicy(
        unsigned numOfRetries,
        nx::MoveOnlyFunc<bool(ResultType)> requestSuccessed);

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

    /**
     * Get the value of the HTTP Date header in milliseconds. Must be accessed from within
     * AIO thread.
     */
    std::chrono::milliseconds lastResponseTime() const { return this->m_lastResponseTime; }

    /**
     * Take the HTTP headers from the most recent response received by the client.
     */
    nx::network::http::HttpHeaders takeLastResponseHeaders()
    {
        return std::exchange(this->m_lastResponseHeaders, {});
    }

    const nx::Url& baseApiUrl() const;

    int getPooledClientsAvailable();

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
        const nx::UrlQuery& urlQuery,
        InputData&& inputData,
        Handler handler);

    // Same as the previous overload, but without input data.
    template<typename OutputData, typename... ResponseFetchers, typename Handler>
    void makeAsyncCall(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::UrlQuery& urlQuery,
        Handler handler);
    template<typename Output, typename InputTuple, typename Handler,
        typename SerializationLibWrapper = detail::NxReflectWrapper, typename... ResponseFetchers>
    void makeAsyncCallWithRetries(
        const network::http::Method& method,
        const std::string& requestPath,
        const nx::UrlQuery& urlQuery,
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

    // The second template parameter here is to use a partial specialization instead of an explicit
    // one for the case when reply type is "void".
    template<typename T, typename Dummy = void>
    struct CacheEntry: BaseCacheEntry
    {
        T reply;
    };

    // Special case - reply type is "void". Use partial specialization to avoid GCC error
    // "explicit specialization in non-namespace scope".
    template<typename Dummy>
    struct CacheEntry<void, Dummy>: BaseCacheEntry
    {
    };

    const nx::Url m_baseApiUrl;
    ssl::AdapterFunc m_adapterFunc;
    std::unordered_map<network::aio::BasicPollable*, Context> m_activeRequests;
    nx::Mutex m_mutex;
    std::optional<std::chrono::milliseconds> m_requestTimeout;
    unsigned int m_numRetries = 1;
    std::optional<nx::MoveOnlyFunc<bool(ResultType)>> m_isRequestSucceeded;
    ClientOptions m_clientOptions;
    bool m_cacheEnabled = false;
    // Actual value has type CacheEntry<T> where T is passed via OutputData template parameter.
    std::unordered_map<std::string /*GET <path>*/, std::shared_ptr<const BaseCacheEntry>> m_cache;
    std::chrono::milliseconds m_lastResponseTime{0};
    nx::network::http::HttpHeaders m_lastResponseHeaders;

    std::deque<std::shared_ptr<nx::network::http::AsyncClient>> m_idleClients;
    int m_idleConnectionsLimit = 0;
    Qn::SerializationFormat m_serializationFormat = Qn::SerializationFormat::json;
    nx::Mutex m_idleClientsMutex;
    nx::utils::AsyncOperationGuard m_asyncOperationGuard;

    HttpClientPtr acquireClient();
    void releaseClient(std::shared_ptr<nx::network::http::AsyncClient> client);
    template<typename Output, typename SerializationLibWrapper, typename... Args>

    auto createHttpClient(HttpClientPtr asyncClient, const Url& url, Args&&... args);
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
    const Url& baseApiUrl,
    ssl::AdapterFunc adapterFunc,
    int idleConnectionsLimit,
    Qn::SerializationFormat sfmt):
    m_baseApiUrl(baseApiUrl),
    m_adapterFunc(std::move(adapterFunc)),
    m_idleConnectionsLimit(idleConnectionsLimit),
    m_serializationFormat(sfmt)
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

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        for (const auto& context: m_activeRequests)
            context.second.client->bindToAioThread(aioThread);
    }
    {
        NX_MUTEX_LOCKER lock(&m_idleClientsMutex);
        for (const auto& client: m_idleClients)
            client->bindToAioThread(aioThread);
    }
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
    unsigned numOfRetries, nx::MoveOnlyFunc<bool(ResultType)> isRequstSucceded)
{
    m_numRetries = numOfRetries;
    m_isRequestSucceeded = std::move(isRequstSucceded);
}

template<HasResultCodeT ResultCode, typename Base>
void GenericApiClient<ResultCode, Base>::setHttpCredentials(Credentials credentials)
{
    m_clientOptions.setCredentials(std::move(credentials));
}

template<HasResultCodeT ResultCode, typename Base>
std::optional<Credentials> GenericApiClient<ResultCode, Base>::getHttpCredentials() const
{
    return !m_clientOptions.credentials().authToken.empty()
        ? std::make_optional(m_clientOptions.credentials()) : std::nullopt;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::stopWhileInAioThread()
{
    // Clear idle clients synchronously before destroying anything to prevent accessing
    // m_idleClientsMutex after it's been destroyed. This ensures that any pending
    // releaseClient() callbacks will either complete before we exit this method, or will
    // safely fail to add clients to the now-empty m_idleClients deque.
    {
        NX_MUTEX_LOCKER lock(&m_idleClientsMutex);
        m_idleClients.clear();
    }

    base_type::stopWhileInAioThread();

    m_activeRequests.clear();
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename... ResponseFetchers, typename InputData, typename Handler>
inline void GenericApiClient<ApiResultCodeDescriptor, Base>::makeAsyncCall(
    const network::http::Method& method,
    const std::string& requestPath,
    const nx::UrlQuery& urlQuery,
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
    const nx::UrlQuery& urlQuery,
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
    const nx::UrlQuery& urlQuery,
    std::tuple<ResponseFetchers...> /*dummy*/,
    InputTuple&& argsTuple,
    unsigned attemptNum,
    Handler handler)
{
    auto httpClient = acquireClient();

    auto request = std::apply(
        [this, &requestPath, &urlQuery, &httpClient]<typename... T>(T&&... inputArgs)
        {
            return this->template createHttpClient<Output, SerializationLibWrapper>(
                std::move(httpClient),
                network::url::Builder(m_baseApiUrl).appendPath(requestPath).setQuery(urlQuery),
                std::forward<T>(inputArgs)...);
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
const nx::Url& GenericApiClient<ApiResultCodeDescriptor, Base>::baseApiUrl() const
{
    return m_baseApiUrl;
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
int GenericApiClient<ApiResultCodeDescriptor, Base>::getPooledClientsAvailable()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_idleConnectionsLimit - m_activeRequests.size();
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
GenericApiClient<ApiResultCodeDescriptor, Base>::HttpClientPtr
    GenericApiClient<ApiResultCodeDescriptor, Base>::acquireClient()
{
    std::shared_ptr<nx::network::http::AsyncClient> asyncClient;
    {
        NX_MUTEX_LOCKER lock(&m_idleClientsMutex);
        if (!m_idleClients.empty())
        {
            asyncClient = m_idleClients.front();
            m_idleClients.pop_front();
        }
    }

    if (!asyncClient)
        asyncClient = std::make_shared<network::http::AsyncClient>(m_adapterFunc);
    else
        asyncClient->assignOptions(m_clientOptions);

    return HttpClientPtr(
        asyncClient.get(),
        [this, asyncClient](network::http::AsyncClient*) mutable
        {
            releaseClient(asyncClient);
        }
    );
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
void GenericApiClient<ApiResultCodeDescriptor, Base>::releaseClient(
    std::shared_ptr<nx::network::http::AsyncClient> client)
{
    NX_ASSERT(client);
    if (!client || m_idleConnectionsLimit <= 0)
    {
        client.reset();
        return;
    }
    const auto sharedGuard = m_asyncOperationGuard.sharedGuard();
    client->cancelPostedCalls(
        [this, client, sharedGuard]() mutable
        {
            if (auto asyncLock = sharedGuard->lock())
            {
                NX_MUTEX_LOCKER lock(&m_idleClientsMutex);
                if ((int) m_idleClients.size() < m_idleConnectionsLimit)
                {
                    m_idleClients.push_back(std::move(client));
                }
            }
        });
}

template<HasResultCodeT ApiResultCodeDescriptor, typename Base>
template<typename Output, typename SerializationLibWrapper, typename... Args>
auto GenericApiClient<ApiResultCodeDescriptor, Base>::createHttpClient(
    HttpClientPtr asyncClient,
    const Url& url,
    Args&&... args)
{
    using InputParamType = std::tuple_element_t<0, std::tuple<std::decay_t<Args>..., void>>;

    auto httpClient = std::make_unique<
        network::http::FusionDataHttpClient<InputParamType, Output, SerializationLibWrapper>>(
        m_serializationFormat,
        url,
        http::Credentials(),
        std::move(asyncClient),
        std::forward<Args>(args)...);
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
std::shared_ptr<const typename GenericApiClient<ApiResultCodeDescriptor, Base>::template CacheEntry<CachedType, void>>
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
    const auto date = http::getHeaderValue(
        response ? response->headers : network::http::HttpHeaders{}, "Date");
    m_lastResponseTime = date.empty()
        ? std::chrono::milliseconds(0)
        : std::chrono::milliseconds(nx::utils::parseDateToMillisSinceEpoch(date));
    m_lastResponseHeaders = response
        ? std::move(response->headers)
        : nx::network::http::HttpHeaders{};

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

    network::http::Response responseVal;
    if (response)
        responseVal = std::move(*response);
    // Call releaseClient before calling handler to make sure <this> is still a valid ptr.
    ctx.client.reset();
    handler(resultCode, std::move(output)..., ResponseFetchers()(responseVal)...);
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
