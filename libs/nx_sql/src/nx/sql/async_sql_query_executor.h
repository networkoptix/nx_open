// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <future>
#include <functional>
#include <memory>

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/thread.h>
#include <nx/utils/type_traits.h>
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

    virtual void pleaseStopSync() = 0;

    virtual const ConnectionOptions& connectionOptions() const = 0;

    /**
     * By default, each query type has same priority of kDefaultQueryPriority.
     */
    virtual void setQueryPriority(QueryType queryType, int newPriority) = 0;

    /**
     * @param value Zero - no limit. By default, zero.
     */
    virtual void setConcurrentModificationQueryLimit(int value) = 0;

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

    virtual int pendingQueryCount() const = 0;

    /**
     * Convenience overload for executeUpdate where DbFunc returns void or throws an exception
     * @param queryAggregationKey Queries with same non-empty value of this parameter
     * can be executed together under single transaction.
     */
    template<
        typename DbFunc,
        typename =
            std::enable_if_t<std::is_same_v<std::invoke_result_t<DbFunc, QueryContext*>, void>>>
    void executeUpdate(
        DbFunc dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string())
    {
        executeUpdate(
            [dbUpdateFunc = std::move(dbUpdateFunc)](auto queryContext) mutable -> DBResult
            {
                dbUpdateFunc(queryContext);
                return DBResult::ok;
            },
            std::move(completionHandler),
            queryAggregationKey);
    }

    /**
     * Convenience overload for executeUpdateWithoutTran where DbFunc returns void or throws an
     * exception.
     */
    template<
        typename DbFunc,
        typename =
            std::enable_if_t<std::is_same_v<std::invoke_result_t<DbFunc, QueryContext*>, void>>>
    void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<void(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
    {
        executeUpdateWithoutTran(
            [dbUpdateFunc = std::move(dbUpdateFunc)](auto queryContext) -> DBResult
            {
                dbUpdateFunc(queryContext);
                return DBResult::ok;
            },
            std::move(completionHandler));
    }

    /**
     * Convenience overload for executeSelect where DbFunc returns void or throws an exception.
     */
    template<
        typename DbFunc,
        typename =
            std::enable_if_t<std::is_same_v<std::invoke_result_t<DbFunc, QueryContext*>, void>>>
    void executeSelect(
        DbFunc dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
    {
        executeSelect(
            [dbUpdateFunc = std::move(dbUpdateFunc)](auto queryContext) -> DBResult
            {
                dbUpdateFunc(queryContext);
                return DBResult::ok;
            },
            std::move(completionHandler));
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    virtual DBResult execSqlScriptSync(
        const QByteArray& script,
        nx::sql::QueryContext* const queryContext) = 0;

    /**
     * @param queryFunc throws nx::sql::Exception.
     * @throws nx::sql::Exception.
     */
    template<
        typename QueryFunc,
        typename Result = decltype(std::declval<QueryFunc>()((nx::sql::QueryContext*) nullptr))
    >
    Result executeUpdateQuerySync(QueryFunc queryFunc)
    {
        std::promise<nx::sql::DBResult> queryDonePromise;
        std::promise<Result> resultAvailable;
        executeUpdate(
            [this, &queryFunc, &resultAvailable](nx::sql::QueryContext* queryContext)
            {
                // TODO: #akolesnikov this function is a work-around for msvc bug
                // (if constexpr ... else ...) inside lambda inside template function.
                this->invokeQueryFunc<Result>(queryContext, queryFunc, resultAvailable);
                return nx::sql::DBResult::ok;
            },
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });

        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::sql::DBResult::ok)
            throw nx::sql::Exception(dbResult);

        return resultAvailable.get_future().get();
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
    typename std::invoke_result_t<QueryFunc, nx::sql::QueryContext*>
        executeSelectQuerySync(QueryFunc queryFunc)
    {
        std::invoke_result_t<QueryFunc, nx::sql::QueryContext*> result;

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

    //---------------------------------------------------------------------------------------------
    // Cursor operations.

    template<typename Record>
    void createCursor(
        nx::utils::MoveOnlyFunc<void(SqlQuery*)> prepareCursorFunc,
        nx::utils::MoveOnlyFunc<void(SqlQuery*, Record*)> readRecordFunc,
        nx::utils::MoveOnlyFunc<void(DBResult, QnUuid /*cursorId*/)> completionHandler)
    {
        auto cursorHandler = std::make_unique<detail::CursorHandler<Record>>(
            std::move(prepareCursorFunc),
            std::move(readRecordFunc),
            std::move(completionHandler));

        createCursorImpl(std::move(cursorHandler));
    }

    virtual void createCursorImpl(std::unique_ptr<detail::AbstractCursorHandler> cursorHandler) = 0;

    /**
     * @param id Provided by createCursor.
     */
    template<typename Record>
    void fetchNextRecordFromCursor(
        QnUuid id,
        nx::utils::MoveOnlyFunc<void(DBResult, Record)> completionHandler)
    {
        auto task = std::make_unique<detail::FetchNextRecordFromCursorTask<Record>>(
            id,
            std::move(completionHandler));
        fetchNextRecordFromCursorImpl(std::move(task));
    }

    virtual void fetchNextRecordFromCursorImpl(
        std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> task) = 0;

    /**
     * @param id Provided by createCursor.
     */
    virtual void removeCursor(QnUuid id) = 0;

    virtual int openCursorCount() const = 0;

private:
    // TODO: #akolesnikov: remove this function after switching to msvc2019.
    template<typename Result, typename QueryFunc>
    void invokeQueryFunc(
        nx::sql::QueryContext* queryContext,
        QueryFunc& queryFunc,
        std::promise<Result>& resultAvailable)
    {
        if constexpr (std::is_same_v<Result, void>)
        {
            queryFunc(queryContext);
            resultAvailable.set_value();
        }
        else
        {
            resultAvailable.set_value(queryFunc(queryContext));
        }
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

    virtual void pleaseStopSync() override;

    virtual const ConnectionOptions& connectionOptions() const override;

    virtual void setQueryPriority(QueryType queryType, int newPriority) override;

    virtual void setConcurrentModificationQueryLimit(int value) override;

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

    virtual int pendingQueryCount() const override;

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    const StatisticsCollector& statisticsCollector() const;

    void reserveConnections(int count);

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

    virtual void createCursorImpl(
        std::unique_ptr<detail::AbstractCursorHandler> cursorHandler) override;

    virtual void fetchNextRecordFromCursorImpl(
        std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> executor) override;

    virtual void removeCursor(QnUuid id) override;

    virtual int openCursorCount() const override;

private:
    struct CursorProcessorContext
    {
        detail::CursorHandlerPool cursorContextPool;
        std::unique_ptr<detail::BaseQueryExecutor> processingThread;
    };

    const ConnectionOptions m_connectionOptions;
    mutable nx::Mutex m_mutex;
    StatisticsCollector m_statisticsCollector;
    detail::QueryQueue m_queryQueue;
    std::vector<std::unique_ptr<detail::BaseQueryExecutor>> m_dbThreadList;
    nx::utils::thread m_dropConnectionThread;
    nx::utils::SyncQueue<std::unique_ptr<detail::BaseQueryExecutor>> m_connectionsToDropQueue;
    bool m_terminated;

    detail::QueryQueue m_cursorTaskQueue;
    std::vector<std::unique_ptr<CursorProcessorContext>> m_cursorProcessorContexts;

    bool isNewConnectionNeeded(const nx::Locker<nx::Mutex>& /*lk*/) const;

    void openNewConnection(const nx::Locker<nx::Mutex>& /*lk*/);

    void saveOpenedConnection(
        const nx::Locker<nx::Mutex>& lock,
        std::unique_ptr<detail::BaseQueryExecutor> connection);

    std::unique_ptr<detail::BaseQueryExecutor> createNewConnectionThread(
        const ConnectionOptions& connectionOptions,
        detail::QueryQueue* const queryQueue);

    void dropExpiredConnectionsThreadFunc();
    void reportQueryCancellation(std::unique_ptr<detail::AbstractExecutor>);
    void onConnectionClosed(detail::BaseQueryExecutor* const executorThreadPtr);
    void dropConnectionAsync(
        const nx::Locker<nx::Mutex>&,
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
        NX_MUTEX_LOCKER lk(&m_mutex);

        if (isNewConnectionNeeded(lk))
            openNewConnection(lk);

        auto executor = std::make_unique<Executor>(
            std::move(updateFunc),
            std::move(input)...,
            std::move(completionHandler),
            queryAggregationKey);

        executor->setStatisticsCollector(&m_statisticsCollector);

        m_queryQueue.push(std::move(executor));
    }

    void addCursorProcessingThread(const nx::Locker<nx::Mutex>& lock);
};

} // namespace nx::sql
