// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <functional>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/stoppable.h>

#include <nx/cloud/db/api/result_code.h>

#include "data/types.h"

namespace nx::cloud::db::client {

template <typename... Args>
void serializeToUrlQuery(Args&&...)
{
    NX_ASSERT(false);
}

/**
 * Executes HTTP requests asynchronously.
 * On object destruction all not yet completed requests are cancelled.
 */
class AsyncRequestsExecutor:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;

public:
    AsyncRequestsExecutor(
        network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher);
    virtual ~AsyncRequestsExecutor();

    virtual void bindToAioThread(network::aio::AbstractAioThread* aioThread) override;

    void setCredentials(nx::network::http::Credentials credentials);

    void setProxyCredentials(nx::network::http::Credentials credentials);

    void setProxyVia(
        const nx::network::SocketAddress& proxyEndpoint,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        bool isSecure);

    void setRequestTimeout(std::chrono::milliseconds);
    std::chrono::milliseconds requestTimeout() const;

    void setAdditionalHeaders(nx::network::http::HttpHeaders headers);
    nx::network::http::HttpHeaders additionalHeaders() const;

    /**
     * Asynchronously fetches url and executes request.
     */
    template<
        typename Output,
        typename... Fetchers,
        typename Input,
        typename Handler
    >
    void executeRequest(
        const nx::network::http::Method& httpMethod,
        const std::string& path,
        Input input,
        Handler handler)
    {
        nx::network::http::AuthInfo auth;
        nx::network::ssl::AdapterFunc proxyAdapterFunc;
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            auth = m_auth;
            proxyAdapterFunc = m_proxyAdapterFunc;
        }

        m_cdbEndPointFetcher->get(auth, proxyAdapterFunc,
            [this, auth, proxyAdapterFunc, httpMethod, path, input = std::move(input),
            handler = std::move(handler)](
                nx::network::http::StatusCode::Value resCode, nx::utils::Url cdbUrl) mutable
            {
                post(
                    [this, resCode, cdbUrl = std::move(cdbUrl), auth = std::move(auth),
                    proxyAdapterFunc = std::move(proxyAdapterFunc), httpMethod, path,
                    input = std::move(input), handler = std::move(handler)]() mutable
                    {
                        if (resCode != nx::network::http::StatusCode::ok)
                        {
                            if constexpr (std::is_void_v<Output>)
                                return handler(api::httpStatusCodeToResultCode(resCode), typename Fetchers::type()...);
                            else
                                return handler(api::httpStatusCodeToResultCode(resCode), Output(), typename Fetchers::type()...);
                        }

                        cdbUrl.setPath(network::url::normalizePath(cdbUrl.path().toStdString() + path));
                        execute<Input, Output, Fetchers...>(
                            httpMethod,
                            std::move(cdbUrl),
                            std::move(auth),
                            std::move(proxyAdapterFunc),
                            input,
                            std::move(handler));
                    });
            });
    }

    template<
        typename Output,
        typename... Fetchers,
        typename Handler
    >
    void executeRequest(
        const nx::network::http::Method& httpMethod,
        const std::string& path,
        Handler handler)
    {
        nx::network::http::AuthInfo auth;
        nx::network::ssl::AdapterFunc proxyAdapterFunc;
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            auth = m_auth;
            proxyAdapterFunc = m_proxyAdapterFunc;
        }

        m_cdbEndPointFetcher->get(auth, proxyAdapterFunc,
            [this, auth, proxyAdapterFunc, httpMethod, path, handler = std::move(handler)](
                nx::network::http::StatusCode::Value resCode, nx::utils::Url cdbUrl) mutable
            {
                post(
                    [this, resCode, cdbUrl = std::move(cdbUrl), auth = std::move(auth),
                    proxyAdapterFunc = std::move(proxyAdapterFunc), httpMethod, path,
                    handler = std::move(handler)]() mutable
                    {
                        if (resCode != nx::network::http::StatusCode::ok)
                        {
                            if constexpr (std::is_void_v<Output>)
                                return handler(api::httpStatusCodeToResultCode(resCode));
                            else
                                return handler(api::httpStatusCodeToResultCode(resCode), Output(), typename Fetchers::type()...);
                        }

                        cdbUrl.setPath(network::url::normalizePath(cdbUrl.path().toStdString() + path));
                        execute<Output, Fetchers...>(
                            httpMethod,
                            std::move(cdbUrl),
                            std::move(auth),
                            std::move(proxyAdapterFunc),
                            std::move(handler));
                    });
            });
    }

    template<typename... Fetchers, typename Handler>
    void executeRequest(
        const std::string& path,
        Handler handler)
    {
        executeRequest<Fetchers...>(
            nx::network::http::Method::get,
            path,
            std::move(handler));
    }

protected:
    virtual void stopWhileInAioThread() override;

private:
    mutable nx::Mutex m_mutex;
    nx::network::http::AuthInfo m_auth;
    nx::network::ssl::AdapterFunc m_proxyAdapterFunc = nx::network::ssl::kDefaultCertificateCheck;
    std::deque<std::unique_ptr<network::aio::BasicPollable>> m_runningRequests;
    std::unique_ptr<
        network::cloud::CloudModuleUrlFetcher::ScopedOperation
    > m_cdbEndPointFetcher;
    std::chrono::milliseconds m_requestTimeout;
    nx::network::http::HttpHeaders m_additionalHeaders;

    /**
     * @param completionHandler Can be invoked within this call if failed to initiate async request.
     */
    template<
        typename Input,
        typename Output,
        typename... Fetchers,
        typename Handler
    >
    void execute(
        const nx::network::http::Method& httpMethod,
        nx::utils::Url url,
        nx::network::http::AuthInfo auth,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        const Input& input,
        Handler handler)
    {
        if (nx::network::http::Method::isMessageBodyAllowed(httpMethod))
        {
            execute<Output, Fetchers...>(
                httpMethod,
                std::make_unique<nx::network::http::FusionDataHttpClient<Input, Output>>(
                    std::move(url),
                    std::move(auth),
                    nx::network::ssl::kDefaultCertificateCheck,
                    std::move(proxyAdapterFunc),
                    input),
                std::move(handler));
        }
        else
        {
            QUrlQuery query(url.query());
            serializeToUrlQuery(input, &query);
            url.setQuery(query);

            execute<Output, Fetchers...>(
                httpMethod,
                std::make_unique<nx::network::http::FusionDataHttpClient<void, Output>>(
                    std::move(url),
                    std::move(auth),
                    nx::network::ssl::kDefaultCertificateCheck,
                    std::move(proxyAdapterFunc)),
                std::move(handler));
        }
    }

    template<
        typename Output,
        typename... Fetchers,
        typename Handler
    >
    void execute(
        const nx::network::http::Method& httpMethod,
        nx::utils::Url url,
        nx::network::http::AuthInfo auth,
        nx::network::ssl::AdapterFunc proxyAdapterFunc,
        Handler handler)
    {
        execute<Output, Fetchers...>(
            httpMethod,
            std::make_unique<nx::network::http::FusionDataHttpClient<void, Output>>(
                std::move(url),
                std::move(auth),
                nx::network::ssl::kDefaultCertificateCheck,
                std::move(proxyAdapterFunc)),
            std::move(handler));
    }

    template<typename... Args> struct VariadicPack {};

    template<typename Output, typename... Fetchers, typename HttpClientType, typename Handler>
    void execute(
        const nx::network::http::Method& httpMethod,
        std::unique_ptr<HttpClientType> client,
        Handler handler)
    {
        if constexpr (std::is_void_v<Output>)
        {
            execute(
                VariadicPack<>(),
                VariadicPack<Fetchers...>(),
                httpMethod,
                std::move(client),
                std::move(handler));
        }
        else
        {
            execute(
                VariadicPack<Output>(),
                VariadicPack<Fetchers...>(),
                httpMethod,
                std::move(client),
                std::move(handler));
        }
    }

    template<typename... Output, typename... Fetchers, typename HttpClientType, typename Handler>
    void execute(
        const VariadicPack<Output...> /*dummy*/,
        const VariadicPack<Fetchers...> /*dummy*/,
        const nx::network::http::Method& httpMethod,
        std::unique_ptr<HttpClientType> client,
        Handler handler)
    {
        client->bindToAioThread(getAioThread());

        client->setRequestTimeout(m_requestTimeout);
        client->httpClient().setAdditionalHeaders(additionalHeaders());

        m_runningRequests.push_back(std::unique_ptr<network::aio::BasicPollable>());
        auto thisClient = client.get();
        client->execute(
            httpMethod,
            [handler = std::move(handler), this, thisClient](
                SystemError::ErrorCode errCode,
                const nx::network::http::Response* response,
                Output... output)
            {
                auto client = getClientByPointer(thisClient);
                if (!client)
                    return; //< Request has been cancelled...

                if ((errCode != SystemError::noError && errCode != SystemError::invalidData)
                    || !response)
                {
                    return handler(
                        api::ResultCode::networkError,
                        Output()...,
                        typename Fetchers::type()...);
                }

                const api::ResultCode resultCode = getResultCode(response);
                handler(resultCode, std::move(output)..., Fetchers()(*response)...);
            });
        m_runningRequests.back() = std::move(client);
    }

    std::unique_ptr<network::aio::BasicPollable> getClientByPointer(
        network::aio::BasicPollable* httpClientPtr)
    {
        auto requestIter =
            std::find_if(
                m_runningRequests.begin(),
                m_runningRequests.end(),
                [httpClientPtr](
                    const std::unique_ptr<network::aio::BasicPollable>& client)
                {
                    return client.get() == httpClientPtr;
                });
        if (requestIter == m_runningRequests.end())
            return nullptr;
        auto client = std::move(*requestIter);
        m_runningRequests.erase(requestIter);
        return client;
    }

    api::ResultCode getResultCode(const nx::network::http::Response* response)
    {
        const auto resultCodeStrIter =
            response->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
        if (resultCodeStrIter != response->headers.end())
        {
            return nx::reflect::fromString(
                resultCodeStrIter->second, api::ResultCode::unknownError);
        }
        else
        {
            return api::httpStatusCodeToResultCode(
                static_cast<nx::network::http::StatusCode::Value>(
                    response->statusLine.statusCode));
        }
    }
};

} // namespace nx::cloud::db::client
