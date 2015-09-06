/**********************************************************
* Sep 5, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
#define NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H

#include <deque>

#include <QtCore/QUrl>

#include "data/account_data.h"
#include "include/cdb/account_manager.h"
#include "utils/thread/mutex.h"


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
    AsyncRequestsExecutor();
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
        const auto result = execute(
            std::make_unique<FusionDataHttpClient<InputData, OutputData>>(
                std::move(url),
                input,
                [completionHandler, requestIter, this](api::ResultCode result, OutputData data)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        m_runningRequests.erase(requestIter);
                    }
                    completionHandler(result, std::move(data));
                }));
        if (!result)
            completionHandler(api::ResultCode::networkError, OutputData());
    }

    template<typename InputData>
    void execute(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode)> completionHandler)
    {
        const auto result = execute(
            std::make_unique<FusionDataHttpClient<InputData, void>>(
                std::move(url),
                input,
                [completionHandler, requestIter, this](api::ResultCode result)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        m_runningRequests.erase(requestIter);
                    }
                    completionHandler(result);
                }));
        if (!result)
            completionHandler(api::ResultCode::networkError);
    }

    template<typename OutputData>
    void execute(
        QUrl url,
        std::function<void(api::ResultCode, OutputData)> completionHandler)
    {
        const auto result = execute(
            std::make_unique<FusionDataHttpClient<void, OutputData>>(
                std::move(url),
                [completionHandler, requestIter, this](api::ResultCode result, OutputData data)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        m_runningRequests.erase(requestIter);
                    }
                    completionHandler(result, std::move(data));
                }));
        if (!result)
            completionHandler(api::ResultCode::networkError, OutputData());
    }

    void execute(
        QUrl url,
        std::function<void(api::ResultCode)> completionHandler)
    {
        const auto result = execute(
            std::make_unique<FusionDataHttpClient<void, void>>(
                std::move(url),
                [completionHandler, requestIter, this](api::ResultCode result)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        m_runningRequests.erase(requestIter);
                    }
                    completionHandler(result);
                }));
        if (!result)
            completionHandler(api::ResultCode::networkError);
    }

private:
    QnMutex m_mutex;
    std::deque<std::unique_ptr<QnStoppableAsync>> m_runningRequests;

    //!Returns \a false if failed to start async request
    template<typename HttpClientType>
    bool execute(std::unique_ptr<HttpClientType> client)
    {
        QnMutexLocker lk(&m_mutex);
        m_runningRequests.push_back(std::unique_ptr<Qn::StoppableAsync>());
        auto requestIter = std::prev(m_runningRequests.end());
        m_runningRequests.back() = std::move(client);
        if (m_runningRequests.back()->get())
            return true;
        m_runningRequests.pop_back();
        return false;
    }
};


}   //cl
}   //cdb
}   //nx

#endif	//NX_CDB_CL_ASYNC_REQUESTS_EXECUTOR_H
