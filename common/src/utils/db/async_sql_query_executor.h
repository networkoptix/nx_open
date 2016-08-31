/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_DB_MANAGER_H
#define NX_CLOUD_DB_DB_MANAGER_H

#include <atomic>
#include <deque>
#include <functional>
#include <memory>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/threadqueue.h>

#include "request_execution_thread.h"
#include "request_executor.h"
#include "types.h"


namespace nx {
namespace db {

/** Executes DB request.
    Scales DB operations on multiple threads
*/
class AsyncSqlQueryExecutor
{
public:
    AsyncSqlQueryExecutor(const ConnectionOptions& connectionOptions);
    virtual ~AsyncSqlQueryExecutor();

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    /** Executes data modification request that spawns some output data.
        Hold multiple threads inside. \a dbUpdateFunc is executed within random thread.
        Transaction is started before \a dbUpdateFunc call.
        Transaction committed if \a dbUpdateFunc succeeded.
        \param dbUpdateFunc This function may executed SQL commands and fill output data
        \param completionHandler DB operation result is passed here. Output data is valid only if operation succeeded
        \note DB operation may fail even if \a dbUpdateFunc finished successfully (e.g., transaction commit fails)
    */
    template<typename InputData, typename OutputData>
    void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*, const InputData&, OutputData* const)> dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData, OutputData)> completionHandler)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<UpdateWithOutputExecutor<InputData, OutputData>>(
            std::move(dbUpdateFunc),
            std::move(input),
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }

    /** Overload for updates with no output data. */
    template<typename InputData>
    void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*, const InputData&)> dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, InputData)> completionHandler)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<UpdateExecutor<InputData>>(
            std::move(dbUpdateFunc),
            std::move(input),
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }

    /** Overload for updates with no input data. */
    void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> completionHandler)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<UpdateWithoutAnyDataExecutor>(
            std::move(dbUpdateFunc),
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }

    void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult)> completionHandler)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<UpdateWithoutAnyDataExecutorNoTran>(
            std::move(dbUpdateFunc),
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }

    template<typename OutputData>
    void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(QSqlDatabase*, OutputData* const)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(QSqlDatabase*, DBResult, OutputData)> completionHandler)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<SelectExecutor<OutputData>>(
            std::move(dbSelectFunc),
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }

    const ConnectionOptions& connectionOptions() const
    {
        return m_connectionOptions;
    }

private:
    const ConnectionOptions m_connectionOptions;
    mutable QnMutex m_mutex;
    CLThreadQueue<std::unique_ptr<AbstractExecutor>> m_requestQueue;
    std::vector<std::unique_ptr<DbRequestExecutionThread>> m_dbThreadPool;
    size_t m_connectionsBeingAdded;
    nx::utils::thread m_dropConnectionThread;
    CLThreadQueue<std::unique_ptr<DbRequestExecutionThread>> m_connectionsToDropQueue;

    /**
        @return \a true if no new connection is required or new connection has been opened.
            \a false in case of failure to open connection when required
    */
    bool openOneMoreConnectionIfNeeded();
    void dropClosedConnections(QnMutexLockerBase* const lk);
    void dropExpiredConnectionsThreadFunc();
};

}   //db
}   //nx

#endif  //NX_CLOUD_DB_DB_MANAGER_H
