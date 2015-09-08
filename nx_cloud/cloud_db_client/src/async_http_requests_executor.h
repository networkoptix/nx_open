/**********************************************************
* Sep 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
#define NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H

#include <deque>
#include <functional>

#include <QtCore/QUrl>

#include <utils/common/cpp14.h>
#include <utils/common/stoppable.h>
#include <utils/network/http/fusion_data_http_client.h>
#include <utils/thread/mutex.h>

#include "data/account_data.h"
#include "data/types.h"
#include "include/cdb/account_manager.h"
#include "include/cdb/result_code.h"
#include "cdb_endpoint_fetcher.h"


namespace nx {
namespace cdb {
namespace cl {


//!Executes HTTP requests asynchronously
/*!
    On object destruction all not yet completed requests are cancelled
*/ 
class AsyncRequestsExecutor
{
public:
    AsyncRequestsExecutor(CloudModuleEndPointFetcher* const cdbEndPointFetcher)
    :
        m_cdbEndPointFetcher(cdbEndPointFetcher)
    {
    }

    virtual ~AsyncRequestsExecutor()
    {
        QnMutexLocker lk(&m_mutex);
        while(!m_runningRequests.empty())
        {
            auto request = std::move(m_runningRequests.front());
            m_runningRequests.pop_front();
            lk.unlock();
            request->join();
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
            std::move(username),
            std::move(password),
            [this, path, input, handler, errHandler](
                api::ResultCode resCode,
                SocketAddress endpoint) mutable
        {
            if (resCode != api::ResultCode::ok)
                return errHandler(resCode);
            QUrl url;
            url.setScheme("http");
            url.setHost(endpoint.address.toString());
            url.setPort(endpoint.port);
            url.setPath(url.path() + path);
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
            std::move(username),
            std::move(password),
            [this, path, handler, errHandler](
                api::ResultCode resCode,
                SocketAddress endpoint) mutable
        {
            if (resCode != api::ResultCode::ok)
                return errHandler(resCode);
            QUrl url;
            url.setScheme("http");
            url.setHost(endpoint.address.toString());
            url.setPort(endpoint.port);
            url.setPath(url.path() + path);
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
    CloudModuleEndPointFetcher* const m_cdbEndPointFetcher;

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
    
    //with output
    template<typename HttpClienType, typename OutputData>
    void execute(
        std::unique_ptr<HttpClienType> client,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        //TODO #ak introduce generic implementation with variadic templates available

        QnMutexLocker lk(&m_mutex);
        m_runningRequests.push_back(std::unique_ptr<QnStoppableAsync>());
        auto requestIter = std::prev(m_runningRequests.end());
        const bool result = client->get(
            [completionHandler, requestIter, this](
                SystemError::ErrorCode errCode,
                nx_http::StatusCode::Value statusCode,
                OutputData data)
            {
                {
                    QnMutexLocker lk(&m_mutex);
                    m_runningRequests.erase(requestIter);
                }
                if (errCode != SystemError::noError)
                    return completionHandler(api::ResultCode::networkError, OutputData());
                completionHandler(api::httpStatusCodeToResultCode(statusCode), std::move(data));
            });
        if (result)
        {
            m_runningRequests.back() = std::move(client);
        }
        else
        {
            m_runningRequests.pop_back();
            completionHandler(api::ResultCode::networkError, OutputData());
        }
    }

    //without output
    template<typename HttpClienType>
    void execute(
        std::unique_ptr<HttpClienType> client,
        std::function<void(api::ResultCode)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);
        m_runningRequests.push_back(std::unique_ptr<QnStoppableAsync>());
        auto requestIter = std::prev(m_runningRequests.end());
        const bool result = client->get(
            [completionHandler, requestIter, this](
                SystemError::ErrorCode errCode,
                nx_http::StatusCode::Value statusCode)
            {
                {
                    QnMutexLocker lk(&m_mutex);
                    m_runningRequests.erase(requestIter);
                }
                if (errCode != SystemError::noError)
                    return completionHandler(api::ResultCode::networkError);
                completionHandler(api::httpStatusCodeToResultCode(statusCode));
            });
        if (result)
        {
            m_runningRequests.back() = std::move(client);
        }
        else
        {
            m_runningRequests.pop_back();
            completionHandler(api::ResultCode::networkError);
        }
    }
};


}   //cl
}   //cdb
}   //nx

#endif	//NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
