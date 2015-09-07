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

protected:
    /*!
        \param completionHandler Can be invoked within this call if failed to initiate async request
    */
    template<typename InputData, typename OutputData>
    void execute(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        //TODO #ak introduce generic implementation with variadic templates available

        QnMutexLocker lk(&m_mutex);
        auto client = std::make_unique<
            nx_http::FusionDataHttpClient<InputData, OutputData>>(std::move(url), input);
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

    template<typename InputData>
    void execute(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);
        auto client = std::make_unique<
            nx_http::FusionDataHttpClient<InputData, void>>(std::move(url), input);
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

    template<typename OutputData>
    void execute(
        QUrl url,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);
        auto client = std::make_unique<
            nx_http::FusionDataHttpClient<void, OutputData>>(std::move(url));
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

    void execute(
        QUrl url,
        std::function<void(api::ResultCode)> completionHandler)
    {
        QnMutexLocker lk(&m_mutex);
        auto client = std::make_unique<
            nx_http::FusionDataHttpClient<void, void>>(std::move(url));
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

private:
    QnMutex m_mutex;
    std::deque<std::unique_ptr<QnStoppableAsync>> m_runningRequests;
};


}   //cl
}   //cdb
}   //nx

#endif	//NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
