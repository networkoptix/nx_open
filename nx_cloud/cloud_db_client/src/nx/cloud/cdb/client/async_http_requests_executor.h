#pragma once

#include <chrono>
#include <deque>
#include <functional>

#include <QtCore/QUrl>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/std/cpp14.h>

#include <nx/network/http/custom_headers.h>
#include <nx/utils/thread/stoppable.h>

#include "data/account_data.h"
#include "data/types.h"
#include "include/nx/cloud/cdb/api/account_manager.h"
#include "include/nx/cloud/cdb/api/result_code.h"

namespace nx {
namespace cdb {
namespace client {

/**
 * Executes HTTP requests asynchronously.
 * On object destruction all not yet completed requests are cancelled.
 */
class AsyncRequestsExecutor
{
public:
    AsyncRequestsExecutor(
        network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher);
    virtual ~AsyncRequestsExecutor();

    void setCredentials(
        const std::string& login,
        const std::string& password);
    void setProxyCredentials(
        const std::string& login,
        const std::string& password);
    void setProxyVia(const SocketAddress& proxyEndpoint);

    void setRequestTimeout(std::chrono::milliseconds);
    std::chrono::milliseconds requestTimeout() const;

    /**
     * Asynchronously fetches url and executes request.
     */
    template<typename InputData, typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
        nx_http::Method::ValueType httpMethod,
        const QString& path,
        InputData input,
        HandlerFunc handler,
        ErrHandlerFunc errHandler)
    {
        // TODO #ak Introduce generic implementation with variadic templates available.

        nx_http::AuthInfo auth;
        SocketAddress proxyEndpoint;
        {
            QnMutexLocker lk(&m_mutex);
            auth = m_auth;
        }

        m_cdbEndPointFetcher->get(
            auth,
            [this, auth, httpMethod, path, input, handler, errHandler](
                nx_http::StatusCode::Value resCode,
                nx::utils::Url cdbUrl) mutable
            {
                if (resCode != nx_http::StatusCode::ok)
                    return errHandler(api::httpStatusCodeToResultCode(resCode));

                cdbUrl.setPath(network::url::normalizePath(cdbUrl.path() + path));
                execute(
                    httpMethod,
                    std::move(cdbUrl),
                    std::move(auth),
                    input,
                    std::move(handler));
            });
    }

    template<typename InputData, typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
        const QString& path,
        InputData input,
        HandlerFunc handler,
        ErrHandlerFunc errHandler)
    {
        executeRequest<InputData, HandlerFunc, ErrHandlerFunc>(
            nx_http::Method::post,
            path,
            std::move(input),
            std::move(handler),
            std::move(errHandler));
    }

    template<typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
        nx_http::Method::ValueType httpMethod,
        const QString& path,
        HandlerFunc handler,
        ErrHandlerFunc errHandler)
    {
        nx_http::AuthInfo auth;
        {
            QnMutexLocker lk(&m_mutex);
            auth = m_auth;
        }

        m_cdbEndPointFetcher->get(
            auth,
            [this, auth, httpMethod, path, handler, errHandler](
                nx_http::StatusCode::Value resCode,
                nx::utils::Url cdbUrl) mutable
            {
                if (resCode != nx_http::StatusCode::ok)
                    return errHandler(api::httpStatusCodeToResultCode(resCode));

                cdbUrl.setPath(network::url::normalizePath(cdbUrl.path() + path));
                execute(
                    httpMethod,
                    std::move(cdbUrl),
                    std::move(auth),
                    std::move(handler));
            });
    }

    template<typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
        const QString& path,
        HandlerFunc handler,
        ErrHandlerFunc errHandler)
    {
        executeRequest<HandlerFunc, ErrHandlerFunc>(
            nx_http::Method::get,
            path,
            std::move(handler),
            std::move(errHandler));
    }

private:
    mutable QnMutex m_mutex;
    nx_http::AuthInfo m_auth;
    std::deque<std::unique_ptr<network::aio::BasicPollable>> m_runningRequests;
    std::unique_ptr<
        network::cloud::CloudModuleUrlFetcher::ScopedOperation
    > m_cdbEndPointFetcher;
    std::chrono::milliseconds m_requestTimeout;

    /**
     * @param completionHandler Can be invoked within this call if failed to initiate async request.
     */
    template<typename InputData, typename OutputData>
    void execute(
        nx::utils::Url url,
        nx_http::Method::ValueType httpMethod,
        nx_http::AuthInfo auth,
        const InputData& input,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(
            httpMethod,
            std::make_unique<
                nx_http::FusionDataHttpClient<InputData, OutputData>>(std::move(url), std::move(auth), input),
            std::move(completionHandler));
    }

    template<typename InputData>
    void execute(
        nx_http::Method::ValueType httpMethod,
        nx::utils::Url url,
        nx_http::AuthInfo auth,
        const InputData& input,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(
            httpMethod,
            std::make_unique<
                nx_http::FusionDataHttpClient<InputData, void>>(std::move(url), std::move(auth), input),
            std::move(completionHandler));
    }

    template<typename OutputData>
    void execute(
        nx_http::Method::ValueType httpMethod,
        nx::utils::Url url,
        nx_http::AuthInfo auth,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(
            httpMethod,
            std::make_unique<
                nx_http::FusionDataHttpClient<void, OutputData>>(std::move(url), std::move(auth)),
            std::move(completionHandler));
    }

    void execute(
        nx_http::Method::ValueType httpMethod,
        nx::utils::Url url,
        nx_http::AuthInfo auth,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(
            httpMethod,
            std::make_unique<
                nx_http::FusionDataHttpClient<void, void>>(std::move(url), std::move(auth)),
            std::move(completionHandler));
    }

    template<typename HttpClientType, typename ... OutputData>
    void execute(
        nx_http::Method::ValueType httpMethod,
        std::unique_ptr<HttpClientType> client,
        std::function<void(api::ResultCode, OutputData...)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);

        client->setRequestTimeout(m_requestTimeout);

        m_runningRequests.push_back(std::unique_ptr<network::aio::BasicPollable>());
        auto thisClient = client.get();
        client->execute(
            httpMethod,
            [completionHandler, this, thisClient](
                SystemError::ErrorCode errCode,
                const nx_http::Response* response,
                OutputData ... data)
            {
                auto client = getClientByPointer(thisClient);
                if (!client)
                    return; //< Request has been cancelled...

                if ((errCode != SystemError::noError && errCode != SystemError::invalidData)
                    || !response)
                {
                    return completionHandler(
                        api::ResultCode::networkError,
                        OutputData()...);
                }

                const api::ResultCode resultCode = getResultCode(response);
                completionHandler(resultCode, std::move(data)...);
            });
        m_runningRequests.back() = std::move(client);
    }

    std::unique_ptr<network::aio::BasicPollable> getClientByPointer(
        network::aio::BasicPollable* httpClientPtr)
    {
        QnMutexLocker lk(&m_mutex);
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

    api::ResultCode getResultCode(const nx_http::Response* response)
    {
        const auto resultCodeStrIter =
            response->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
        if (resultCodeStrIter != response->headers.end())
        {
            return QnLexical::deserialized<api::ResultCode>(
                resultCodeStrIter->second,
                api::ResultCode::unknownError);
        }
        else
        {
            return api::httpStatusCodeToResultCode(
                static_cast<nx_http::StatusCode::Value>(
                    response->statusLine.statusCode));
        }
    }
};

} // namespace client
} // namespace cdb
} // namespace nx
