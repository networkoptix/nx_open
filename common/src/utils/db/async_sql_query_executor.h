#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include "request_execution_thread.h"
#include "request_executor.h"
#include "types.h"
#include "query_context.h"

namespace nx {
namespace db {

/**
 * Executes DB request asynchronously.
 * Scales DB operations on multiple threads
 */
class AsyncSqlQueryExecutor
{
public:
    AsyncSqlQueryExecutor(const ConnectionOptions& connectionOptions);
    virtual ~AsyncSqlQueryExecutor();

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    /**
     * Executes data modification request that spawns some output data.
     * Hold multiple threads inside. \a dbUpdateFunc is executed within random thread.
     * Transaction is started before \a dbUpdateFunc call.
     * Transaction committed if \a dbUpdateFunc succeeded.
     * @param dbUpdateFunc This function may executed SQL commands and fill output data
     * @param completionHandler DB operation result is passed here. Output data is valid only if operation succeeded
     * @note DB operation may fail even if \a dbUpdateFunc finished successfully (e.g., transaction commit fails).
     * @note \a dbUpdateFunc may not be called if there was no connection available for 
     *       \a ConnectionOptions::maxPeriodQueryWaitsForAvailableConnection period.
     */
    template<typename InputData, typename OutputData>
    void executeUpdate(
        nx::utils::MoveOnlyFunc<
            DBResult(nx::db::QueryContext*, const InputData&, OutputData* const)
        > dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<
            void(nx::db::QueryContext*, DBResult, InputData, OutputData)
        > completionHandler)
    {
        scheduleQuery<UpdateWithOutputExecutor<InputData, OutputData>>(
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            std::move(input));
    }

    /**
     * Overload for updates with no output data.
     */
    template<typename InputData>
    void executeUpdate(
        nx::utils::MoveOnlyFunc<
            DBResult(nx::db::QueryContext*, const InputData&)> dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<
            void(nx::db::QueryContext*, DBResult, InputData)> completionHandler)
    {
        scheduleQuery<UpdateExecutor<InputData>>(
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            std::move(input));
    }

    /**
     * Overload for updates with no input data.
     */
    void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, DBResult)> completionHandler)
    {
        scheduleQuery<UpdateWithoutAnyDataExecutor>(
            std::move(dbUpdateFunc),
            std::move(completionHandler));
    }

    void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::db::QueryContext*, DBResult)> completionHandler)
    {
        scheduleQuery<UpdateWithoutAnyDataExecutorNoTran>(
            std::move(dbUpdateFunc),
            std::move(completionHandler));
    }

    template<typename OutputData>
    void executeSelect(
        nx::utils::MoveOnlyFunc<
            DBResult(nx::db::QueryContext*, OutputData* const)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<
            void(nx::db::QueryContext*, DBResult, OutputData)> completionHandler)
    {
        scheduleQuery<SelectExecutor<OutputData>>(
            std::move(dbSelectFunc),
            std::move(completionHandler));
    }

    const ConnectionOptions& connectionOptions() const
    {
        return m_connectionOptions;
    }

private:
    const ConnectionOptions m_connectionOptions;
    mutable QnMutex m_mutex;
    nx::utils::SyncQueueWithItemStayTimeout<std::unique_ptr<AbstractExecutor>>
        m_requestQueue;
    std::vector<std::unique_ptr<DbRequestExecutionThread>> m_dbThreadPool;
    nx::utils::thread m_dropConnectionThread;
    CLThreadQueue<std::unique_ptr<DbRequestExecutionThread>> m_connectionsToDropQueue;

    /**
     * @return \a true if no new connection is required or new connection has been opened.
     *         \a false in case of failure to open connection when required.
     */
    bool openOneMoreConnectionIfNeeded();
    void dropClosedConnections(QnMutexLockerBase* const lk);
    void dropExpiredConnectionsThreadFunc();
    void reportQueryCancellation(std::unique_ptr<AbstractExecutor>);

    template<
        typename Executor, typename UpdateFunc,
        typename CompletionHandler, typename ... Input>
    void scheduleQuery(
        UpdateFunc updateFunc,
        CompletionHandler completionHandler,
        Input ... input)
    {
        openOneMoreConnectionIfNeeded();

        auto ctx = std::make_unique<Executor>(
            std::move(updateFunc),
            std::move(input)...,
            std::move(completionHandler));

        QnMutexLocker lk(&m_mutex);
        m_requestQueue.push(std::move(ctx));
    }
};

} // namespace db
} // namespace nx
