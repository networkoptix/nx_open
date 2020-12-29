#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/sync_queue_with_item_stay_timeout.h>

#include "detail/base_query_executor.h"
#include "detail/cursor_handler.h"
#include "detail/query_queue.h"
#include "detail/query_executor.h"
#include "types.h"
#include "query.h"
#include "query_context.h"

namespace nx::sql {

class NX_SQL_API AbstractAsyncSqlQueryExecutor
{
public:
    virtual ~AbstractAsyncSqlQueryExecutor() = default;

    virtual const ConnectionOptions& connectionOptions() const = 0;

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    /**
     * @param queryAggregationKey Queries with same non-empty value of this parameter
     * can be executed together under single transaction.
     */
    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string()) = 0;

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) = 0;

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) = 0;

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::sql::QueryContext* const queryContext) = 0;

    /**
     * @param queryFunc throws nx::sql::Exception.
     * @throws nx::sql::Exception.
     */
    template<typename QueryFunc>
    void executeUpdateQuerySync(QueryFunc queryFunc)
    {
        nx::utils::promise<nx::sql::DBResult> queryDonePromise;
        executeUpdate(
            [&queryFunc](nx::sql::QueryContext* queryContext)
            {
                queryFunc(queryContext);
                return nx::sql::DBResult::ok;
            },
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::sql::DBResult::ok)
            throw nx::sql::Exception(dbResult);
    }

    /**
     * @throws nx::sql::Exception.
     */
    void executeSqlSync(QByteArray sqlStatement);

    /**
     * @param queryFunc throws nx::sql::Exception.
     * @throws nx::sql::Exception.
     */
    template<typename QueryFunc>
    typename std::result_of<QueryFunc(nx::sql::QueryContext*)>::type
        executeSelectQuerySync(QueryFunc queryFunc)
    {
        typename std::result_of<QueryFunc(nx::sql::QueryContext*)>::type result;

        nx::utils::promise<nx::sql::DBResult> queryDonePromise;
        executeSelect(
            [&queryFunc, &result](nx::sql::QueryContext* queryContext)
            {
                result = queryFunc(queryContext);
                return nx::sql::DBResult::ok;
            },
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::sql::DBResult::ok)
            throw nx::sql::Exception(dbResult);

        return result;
    }
};

template<typename InputData, typename OutputData> using UpdateQueryWithOutputFunc =
    nx::utils::MoveOnlyFunc<
        DBResult(nx::sql::QueryContext*, const InputData&, OutputData* const)>;

template<typename InputData, typename OutputData> using UpdateQueryWithOutputCompletionHandler =
    nx::utils::MoveOnlyFunc<
        void(DBResult, InputData, OutputData)>;

template<typename InputData> using UpdateQueryFunc =
    nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*, const InputData&)>;

template<typename InputData> using UpdateQueryCompletionHandler =
    nx::utils::MoveOnlyFunc<void(DBResult, InputData)>;

/**
 * Executes DB request asynchronously.
 * Scales DB operations on multiple threads
 */
class NX_SQL_API AsyncSqlQueryExecutor:
    public AbstractAsyncSqlQueryExecutor
{
public:
    static const int kDefaultQueryPriority;

    AsyncSqlQueryExecutor(const ConnectionOptions& connectionOptions);
    virtual ~AsyncSqlQueryExecutor();

    virtual const ConnectionOptions& connectionOptions() const override;

    /**
     * Overload for updates with no input data.
     */
    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string()) override;

    virtual void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) override;

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) override;

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::sql::QueryContext* const queryContext) override;

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    void setStatisticsCollector(StatisticsCollector* statisticsCollector);

    void reserveConnections(int count);

    /**
     * @param value Zero - no limit. By default, zero.
     */
    void setConcurrentModificationQueryLimit(int value);

    /**
     * By default, each query type has same priority of kDefaultQueryPriority.
     */
    void setQueryPriority(QueryType queryType, int newPriority);

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
        UpdateQueryWithOutputFunc<InputData, OutputData> dbUpdateFunc,
        InputData input,
        UpdateQueryWithOutputCompletionHandler<InputData, OutputData> completionHandler,
        const std::string& queryAggregationKey = std::string())
    {
        scheduleQuery<detail::UpdateWithOutputExecutor<InputData, OutputData>>(
            queryAggregationKey,
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            std::move(input));
    }

    /**
     * Overload for updates with no output data.
     */
    template<typename InputData>
    void executeUpdate(
        UpdateQueryFunc<InputData> dbUpdateFunc,
        InputData input,
        UpdateQueryCompletionHandler<InputData> completionHandler,
        const std::string& queryAggregationKey = std::string())
    {
        scheduleQuery<detail::UpdateExecutor<InputData>>(
            queryAggregationKey,
            std::move(dbUpdateFunc),
            std::move(completionHandler),
            std::move(input));
    }

    template<typename Record>
    void createCursor(
        nx::utils::MoveOnlyFunc<void(SqlQuery*)> prepareCursorFunc,
        nx::utils::MoveOnlyFunc<void(SqlQuery*, Record*)> readRecordFunc,
        nx::utils::MoveOnlyFunc<void(DBResult, QnUuid /*cursorId*/)> completionHandler)
    {
        {
            QnMutexLocker lock(&m_mutex);
            if (m_cursorProcessorContexts.empty())
                addCursorProcessingThread(lock);
        }

        auto cursorHandler = std::make_unique<detail::CursorHandler<Record>>(
            std::move(prepareCursorFunc),
            std::move(readRecordFunc),
            std::move(completionHandler));
        auto cursorCreator = std::make_unique<detail::CursorCreator>(
            &m_cursorProcessorContexts.front()->cursorContextPool,
            std::move(cursorHandler));
        m_cursorTaskQueue.push(std::move(cursorCreator));
    }

    template<typename Record>
    void fetchNextRecordFromCursor(
        QnUuid id,
        nx::utils::MoveOnlyFunc<void(DBResult, Record)> completionHandler)
    {
        auto task = std::make_unique<detail::FetchCursorDataExecutor<Record>>(
            &m_cursorProcessorContexts.front()->cursorContextPool,
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
        std::unique_ptr<detail::BaseQueryExecutor> processingThread;
    };

    const ConnectionOptions m_connectionOptions;
    mutable QnMutex m_mutex;
    detail::QueryQueue m_queryQueue;
    std::vector<std::unique_ptr<detail::BaseQueryExecutor>> m_dbThreadList;
    nx::utils::thread m_dropConnectionThread;
    nx::utils::SyncQueue<std::unique_ptr<detail::BaseQueryExecutor>> m_connectionsToDropQueue;
    bool m_terminated;
    StatisticsCollector* m_statisticsCollector;

    detail::QueryQueue m_cursorTaskQueue;
    std::vector<std::unique_ptr<CursorProcessorContext>> m_cursorProcessorContexts;

    bool isNewConnectionNeeded(const QnMutexLockerBase& /*lk*/) const;

    void openNewConnection(const QnMutexLockerBase& /*lk*/);

    void saveOpenedConnection(
        const QnMutexLockerBase& lock,
        std::unique_ptr<detail::BaseQueryExecutor> connection);

    std::unique_ptr<detail::BaseQueryExecutor> createNewConnectionThread(
        const ConnectionOptions& connectionOptions,
        detail::QueryQueue* const queryQueue);

    void dropExpiredConnectionsThreadFunc();
    void reportQueryCancellation(std::unique_ptr<detail::AbstractExecutor>);
    void onConnectionClosed(detail::BaseQueryExecutor* const executorThreadPtr);
    void dropConnectionAsync(
        const QnMutexLockerBase&,
        detail::BaseQueryExecutor* const executorThreadPtr);

    template<
        typename Executor, typename UpdateFunc,
        typename CompletionHandler, typename ... Input>
    void scheduleQuery(
        const std::string& queryAggregationKey,
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
            std::move(completionHandler),
            queryAggregationKey);

        if (m_statisticsCollector)
            executor->setStatisticsCollector(m_statisticsCollector);

        m_queryQueue.push(std::move(executor));
    }

    void addCursorProcessingThread(const QnMutexLockerBase& lock);
};

} // namespace nx::sql
