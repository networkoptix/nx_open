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
#include <utils/common/cpp14.h>
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
        network::cloud::CloudModuleEndPointFetcher* const cdbEndPointFetcher)
    :
        m_cdbEndPointFetcher(cdbEndPointFetcher)
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
            lk.relock();
        }
    }

    void setCredentials(
        const std::string& login,
        const std::string& password)
    {
        QnMutexLocker lk(&m_mutex);
        m_username = QString::fromStdString(login);
        m_password = QString::fromStdString(password);
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

        QString username;
        QString password;
        {
            QnMutexLocker lk(&m_mutex);
            username = m_username;
            password = m_password;
        }

        m_cdbEndPointFetcher->get(
            [this, username, password, path, input, handler, errHandler](
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
                url.setUserName(username);
                url.setPassword(password);
                execute(
                    std::move(url),
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
        QString username;
        QString password;
        {
            QnMutexLocker lk(&m_mutex);
            username = m_username;
            password = m_password;
        }

        m_cdbEndPointFetcher->get(
            [this, username, password, path, handler, errHandler](
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
                url.setUserName(username);
                url.setPassword(password);
                execute(
                    std::move(url),
                    std::move(handler));
            });
    }

private:
    mutable QnMutex m_mutex;
    QString m_username;
    QString m_password;
    std::deque<std::unique_ptr<QnStoppableAsync>> m_runningRequests;
    network::cloud::CloudModuleEndPointFetcher* const m_cdbEndPointFetcher;

    /*!
        \param completionHandler Can be invoked within this call if failed to initiate async request
    */
    template<typename InputData, typename OutputData>
    void execute(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<InputData, OutputData>>(std::move(url), input),
            std::move(completionHandler));
    }

    template<typename InputData>
    void execute(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<InputData, void>>(std::move(url), input),
            std::move(completionHandler));
    }

    template<typename OutputData>
    void execute(
        QUrl url,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<void, OutputData>>(std::move(url)),
            std::move(completionHandler));
    }

    void execute(
        QUrl url,
        std::function<void(api::ResultCode)> completionHandler)
    {
        execute(std::make_unique<
            nx_http::FusionDataHttpClient<void, void>>(std::move(url)),
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
                if (errCode != SystemError::noError || !response)
                    return completionHandler(
                        api::ResultCode::networkError,
                        OutputData()...);

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
