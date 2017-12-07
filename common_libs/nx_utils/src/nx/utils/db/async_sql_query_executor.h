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

#include "base_request_executor.h"
#include "detail/cursor_handler.h"
#include "detail/query_queue.h"
#include "request_executor.h"
#include "types.h"
#include "query.h"
#include "query_context.h"

namespace nx {
namespace utils {
namespace db {

class NX_UTILS_API AbstractAsyncSqlQueryExecutor
{
public:
    virtual ~AbstractAsyncSqlQueryExecutor() = default;

    virtual const ConnectionOptions& connectionOptions() const = 0;

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) = 0;

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) = 0;

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) = 0;

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext) = 0;

    /**
     * @param queryFunc throws nx::utils::db::Exception.
     * @throws nx::utils::db::Exception.
     */
    template<typename QueryFunc>
    void executeUpdateQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::utils::db::DBResult> queryDonePromise;
        executeUpdate(
            [&queryFunc](nx::utils::db::QueryContext* queryContext)
            {
                queryFunc(queryContext);
                return nx::utils::db::DBResult::ok;
            },
            [&queryDonePromise](
                nx::utils::db::QueryContext*,
                nx::utils::db::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::utils::db::DBResult::ok)
            throw nx::utils::db::Exception(dbResult);
    }

    /**
     * @throws nx::utils::db::Exception.
     */
    void executeSqlSync(QByteArray sqlStatement);

    /**
     * @param queryFunc throws nx::utils::db::Exception.
     * @throws nx::utils::db::Exception.
     */
    template<typename QueryFunc>
    typename std::result_of<QueryFunc(nx::utils::db::QueryContext*)>::type
        executeSelectQuerySync(QueryFunc queryFunc)
    {
        typename std::result_of<QueryFunc(nx::utils::db::QueryContext*)>::type result;

        nx::utils::promise<nx::utils::db::DBResult> queryDonePromise;
        executeSelect(
            [&queryFunc, &result](nx::utils::db::QueryContext* queryContext)
            {
                result = queryFunc(queryContext);
                return nx::utils::db::DBResult::ok;
            },
            [&queryDonePromise](
                nx::utils::db::QueryContext*,
                nx::utils::db::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::utils::db::DBResult::ok)
            throw nx::utils::db::Exception(dbResult);

        return result;
    }
};

/**
 * Executes DB request asynchronously.
 * Scales DB operations on multiple threads
 */
class NX_UTILS_API AsyncSqlQueryExecutor:
    public AbstractAsyncSqlQueryExecutor
{
public:
    AsyncSqlQueryExecutor(const ConnectionOptions& connectionOptions);
    virtual ~AsyncSqlQueryExecutor();

    virtual const ConnectionOptions& connectionOptions() const override;

    /**
     * Overload for updates with no input data.
     */
    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override;

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override;

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler) override;

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::utils::db::QueryContext* const queryContext) override;

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    void setStatisticsCollector(StatisticsCollector* statisticsCollector);

    void reserveConnections(int count);

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);

    std::size_t pendingQueryCount() const;

    /**
     * Executes data modification request that spawns some output data.
     * Hold multiple threads inside. dbUpdateFunc is executed within random thread.
     * Transaction is started before dbUpdateFunc call.
     * Transaction committed if dbUpdateFunc succeeded.
     * @param dbUpdateFunc This function may executed SQL commands and fill output data
     * @param completionHandler DB operation result is passed here. Output data is valid only if operation succeeded
     * NOTE: DB operation may fail even if dbUpdateFunc finished successfully (e.g., transaction commit fails).
     * NOTE: dbUpdateFunc may not be called if there was no connection available for
     *       ConnectionOptions::maxPeriodQueryWaitsForAvailableConnection period.
     */
    template<typename InputData, typename OutputData>
    void executeUpdate(
        nx::utils::MoveOnlyFunc<
            DBResult(nx::utils::db::QueryContext*, const InputData&, OutputData* const)
        > dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<
            void(nx::utils::db::QueryContext*, DBResult, InputData, OutputData)
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
            DBResult(nx::utils::db::QueryContext*, const InputData&)> dbUpdateFunc,
        InputData input,
        nx::utils::MoveOnlyFunc<
            void(nx::utils::db::QueryContext*, DBResult, InputData)> completionHandler)
    {
        scheduleQuery<UpdateExecutor<InputData>>(
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            std::move(input));
    }

    template<typename Record>
    void createCursor(
        MoveOnlyFunc<void(SqlQuery*)> prepareCursorFunc,
        MoveOnlyFunc<void(SqlQuery*, Record*)> readRecordFunc,
        MoveOnlyFunc<void(DBResult, QnUuid /*cursorId*/)> completionHandler)
    {
        {
            QnMutexLocker lock(&m_mutex);
            if (m_cursorProcessorContext.empty())
                addCursorProcessingThread(lock);
        }

        auto cursorHandler = std::make_unique<detail::CursorHandler<Record>>(
            std::move(prepareCursorFunc),
            std::move(readRecordFunc),
            std::move(completionHandler));
        auto cursorCreator = std::make_unique<detail::CursorCreator>(
            &m_cursorProcessorContext.front()->cursorContextPool,
            std::move(cursorHandler));
        m_cursorTaskQueue.push(std::move(cursorCreator));
    }

    template<typename Record>
    void fetchNextRecordFromCursor(
        QnUuid id,
        MoveOnlyFunc<void(DBResult, Record)> completionHandler)
    {
        auto task = std::make_unique<detail::FetchCursorDataExecutor<Record>>(
            &m_cursorProcessorContext.front()->cursorContextPool,
            id,
            std::move(completionHandler));
        m_cursorTaskQueue.push(std::move(task));
    }

    void removeCursor(QnUuid id);

    int openCursorCount() const;

private:
    struct CursorProcessorContext
    {
        detail::CursorHandlerPool cursorContextPool;
        std::unique_ptr<BaseRequestExecutor> processingThread;
    };

    const ConnectionOptions m_connectionOptions;
    mutable QnMutex m_mutex;
    detail::QueryQueue m_queryQueue;
    std::vector<std::unique_ptr<BaseRequestExecutor>> m_dbThreadPool;
    nx::utils::thread m_dropConnectionThread;
    nx::utils::SyncQueue<std::unique_ptr<BaseRequestExecutor>> m_connectionsToDropQueue;
    bool m_terminated;
    StatisticsCollector* m_statisticsCollector;

    detail::QueryQueue m_cursorTaskQueue;
    std::vector<std::unique_ptr<CursorProcessorContext>> m_cursorProcessorContext;

    bool isNewConnectionNeeded(const QnMutexLockerBase& /*lk*/) const;

    void openNewConnection(const QnMutexLockerBase& /*lk*/);
    std::unique_ptr<BaseRequestExecutor> createNewConnectionThread(
        const QnMutexLockerBase& /*lock*/,
        detail::QueryQueue* const queryQueue);
    std::unique_ptr<BaseRequestExecutor> createNewConnectionThread(
        const QnMutexLockerBase& /*lock*/,
        const ConnectionOptions& connectionOptions,
        detail::QueryQueue* const queryQueue);

    void dropExpiredConnectionsThreadFunc();
    void reportQueryCancellation(std::unique_ptr<AbstractExecutor>);
    void onConnectionClosed(BaseRequestExecutor* const executorThreadPtr);
    void dropConnectionAsync(
        const QnMutexLockerBase&,
        BaseRequestExecutor* const executorThreadPtr);

    template<
        typename Executor, typename UpdateFunc,
        typename CompletionHandler, typename ... Input>
    void scheduleQuery(
        UpdateFunc updateFunc,
        CompletionHandler completionHandler,
        Input ... input)
    {
        QnMutexLocker lk(&m_mutex);

        if (isNewConnectionNeeded(lk))
            openNewConnection(lk);

        auto executor = std::make_unique<Executor>(
            std::move(updateFunc),
            std::move(input)...,
            std::move(completionHandler));

        if (m_statisticsCollector)
            executor->setStatisticsCollector(m_statisticsCollector);

        m_queryQueue.push(std::move(executor));
    }

    void addCursorProcessingThread(const QnMutexLockerBase& lock);
};

} // namespace db
} // namespace utils
} // namespace nx
