/**********************************************************
* Sep 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
#define NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H

#include <deque>
#include <functional>

#include <QtCore/QUrl>

#include <http/custom_headers.h>
#include <nx/utils/std/cpp14.h>
#include <utils/common/stoppable.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <nx/utils/thread/mutex.h>

#include "data/account_data.h"
#include "data/types.h"
#include "include/cdb/account_manager.h"
#include "include/cdb/result_code.h"


namespace nx {
namespace cdb {
namespace cl {

/** Executes HTTP requests asynchronously.
    On object destruction all not yet completed requests are cancelled
*/
class AsyncRequestsExecutor
{
public:
    AsyncRequestsExecutor(
        network::cloud::CloudModuleUrlFetcher* const cdbEndPointFetcher)
    :
        m_cdbEndPointFetcher(
            std::make_unique<network::cloud::CloudModuleUrlFetcher::ScopedOperation>(
                cdbEndPointFetcher))
    {
    }

    virtual ~AsyncRequestsExecutor()
    {
        //TODO #ak cancel m_cdbEndPointFetcher->get operation

        QnMutexLocker lk(&m_mutex);
        while(!m_runningRequests.empty())
        {
            auto request = std::move(m_runningRequests.front());
            m_runningRequests.pop_front();
            lk.unlock();
            request->pleaseStopSync();
            request.reset();
            lk.relock();
        }
    }

    void setCredentials(
        const std::string& login,
        const std::string& password)
    {
        QnMutexLocker lk(&m_mutex);
        m_auth.username = QString::fromStdString(login);
        m_auth.password = QString::fromStdString(password);
    }

    void setProxyCredentials(
        const std::string& login,
        const std::string& password)
    {
        QnMutexLocker lk(&m_mutex);
        m_auth.proxyUsername = QString::fromStdString(login);
        m_auth.proxyPassword = QString::fromStdString(password);
    }

    void setProxyVia(const SocketAddress& proxyEndpoint)
    {
        QnMutexLocker lk(&m_mutex);
        m_auth.proxyEndpoint = proxyEndpoint;
    }

protected:
    //!asynchronously fetches url and executes request
    template<typename InputData, typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
        const QString& path,
        InputData input,
        HandlerFunc handler,
        ErrHandlerFunc errHandler)
    {
        //TODO #ak introduce generic implementation with variadic templates available

        nx_http::AuthInfo auth;
        SocketAddress proxyEndpoint;
        {
            QnMutexLocker lk(&m_mutex);
            auth = m_auth;
        }

        m_cdbEndPointFetcher->get(
            auth,
            [this, auth, path, input, handler, errHandler](
                nx_http::StatusCode::Value resCode,
                SocketAddress endpoint) mutable
            {
                if (resCode != nx_http::StatusCode::ok)
                    return errHandler(api::httpStatusCodeToResultCode(resCode));

                QUrl url;
                url.setScheme("http");
                url.setHost(endpoint.address.toString());
                url.setPort(endpoint.port);
                url.setPath(url.path() + path);
                execute(
                    std::move(url),
                    std::move(auth),
                    input,
                    std::move(handler));
            });
    }

    template<typename HandlerFunc, typename ErrHandlerFunc>
    void executeRequest(
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
            [this, auth, path, handler, errHandler](
                nx_http::StatusCode::Value resCode,
                SocketAddress endpoint) mutable
            {
                if (resCode != nx_http::StatusCode::ok)
                    return errHandler(api::httpStatusCodeToResultCode(resCode));

                QUrl url;
                url.setScheme("http");
                url.setHost(endpoint.address.toString());
                url.setPort(endpoint.port);
                url.setPath(url.path() + path);
                execute(
                    std::move(url),
                    std::move(auth),
                    std::move(handler));
            });
    }

private:
    mutable QnMutex m_mutex;
    nx_http::AuthInfo m_auth;
    std::deque<std::unique_ptr<QnStoppableAsync>> m_runningRequests;
    std::unique_ptr<
        network::cloud::CloudModuleUrlFetcher::ScopedOperation
    > m_cdbEndPointFetcher;

    /*!
        \param completionHandler Can be invoked within this call if failed to initiate async request
    */
    template<typename InputData, typename OutputData>
    void execute(
        QUrl url,
        nx_http::AuthInfo auth,
        const InputData& input,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<InputData, OutputData>>(std::move(url), std::move(auth), input),
            std::move(completionHandler));
    }

    template<typename InputData>
    void execute(
        QUrl url,
        nx_http::AuthInfo auth,
        const InputData& input,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<InputData, void>>(std::move(url), std::move(auth), input),
            std::move(completionHandler));
    }

    template<typename OutputData>
    void execute(
        QUrl url,
        nx_http::AuthInfo auth,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<void, OutputData>>(std::move(url), std::move(auth)),
            std::move(completionHandler));
    }

    void execute(
        QUrl url,
        nx_http::AuthInfo auth,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<void, void>>(std::move(url), std::move(auth)),
            std::move(completionHandler));
    }

    template<typename HttpClientType, typename ... OutputData>
    void execute(
        std::unique_ptr<HttpClientType> client,
        std::function<void(api::ResultCode, OutputData...)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);
        m_runningRequests.push_back(std::unique_ptr<QnStoppableAsync>());
        auto thisClient = client.get();
        client->execute(
            [completionHandler, this, thisClient](
                SystemError::ErrorCode errCode,
                const nx_http::Response* response,
                OutputData ... data)
            {
                {
                    QnMutexLocker lk(&m_mutex);
                    auto requestIter =
                        std::find_if(
                            m_runningRequests.begin(),
                            m_runningRequests.end(),
                            [thisClient](const std::unique_ptr<QnStoppableAsync>& client)
                            {
                                return client.get() == thisClient;
                            });
                    if (requestIter == m_runningRequests.end())
                        return; //request has been cancelled...
                    m_runningRequests.erase(requestIter);
                }
                if ((errCode != SystemError::noError && errCode != SystemError::invalidData)
                    || !response)
                {
                    return completionHandler(
                        api::ResultCode::networkError,
                        OutputData()...);
                }

                api::ResultCode resultCode = api::ResultCode::ok;
                const auto resultCodeStrIter =
                    response->headers.find(Qn::API_RESULT_CODE_HEADER_NAME);
                if (resultCodeStrIter != response->headers.end())
                {
                    resultCode = QnLexical::deserialized<api::ResultCode>(
                        resultCodeStrIter->second,
                        api::ResultCode::unknownError);
                }
                else
                {
                    resultCode = api::httpStatusCodeToResultCode(
                        static_cast<nx_http::StatusCode::Value>(
                            response->statusLine.statusCode));
                }
                completionHandler(resultCode, std::move(data)...);
            });
        m_runningRequests.back() = std::move(client);
    }
};

}   //namespace cl
}   //namespace cdb
}   //namespace nx

#endif	//NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
