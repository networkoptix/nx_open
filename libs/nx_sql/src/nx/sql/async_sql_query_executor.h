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

    /**
     * @param enabled If set to false, then ConnectionOptions::maxPeriodQueryWaitsForAvailableConnection
     * timeout is not respected.
     */
    virtual void setQueryTimeoutEnabled(bool enabled) = 0;

    //---------------------------------------------------------------------------------------------
    // Asynchronous operations.

    /**
     * Execute SQL query(-ies) within a single DB transaction.
     * Transaction is started before dbUpdateFunc call.
     * dbUpdateFunc is executed in a random thread of the underlying DB thread pool.
     * @param dbUpdateFunc Executes SQL queries. Allowed to throw.
     * @param completionHandler DB operation result is passed here.
     * @param priority Relative priority of this query in comparison to others.
     * @param queryAggregationKey Queries with same non-empty value of this parameter
     * can be executed together under single transaction.
     */
    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string()) = 0;

    /**
     * Executes dbSelectFunc without starting transaction.
     * A separate executeSelect method was introduced to allow managing priorities of
     * select/update queries and to parallel them separately.
     * NOTE: dbSelectFunc can include any queries. There is no check that only "SELECT ..."
     * is executed here. So, misuse of this function can lead to updates executed without
     * DB transaction resulting in data consistency issues.
     */
    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) = 0;

    /**
     * Syntax sugar. Same as 'executeSelect'.
     */
    void executeUpdateWithoutTran(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler);

    virtual QueryQueueStats queryQueueStatistics() const = 0;
    virtual QueryStatistics queryStatistics() const = 0;

    /**
     * Convenience overload for executeUpdate where DbFunc returns void or throws an exception
     * @param queryAggregationKey Queries with same non-empty value of this parameter
     * can be executed together under single transaction.
     */
    template<
        typename DbFunc,
        typename =
            std::enable_if_t<std::is_same_v<std::invoke_result_t<DbFunc, QueryContext*>, void>>
    >
    void executeUpdate(
        DbFunc dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string())
    {
        executeUpdate(
            [dbUpdateFunc = std::move(dbUpdateFunc)](auto queryContext) mutable -> DBResult
            {
                dbUpdateFunc(queryContext);
                return DBResultCode::ok;
            },
            std::move(completionHandler),
            queryAggregationKey);
    }

    /**
     * Convenience overload for executeUpdate where DbFunc application-level result
     * which is then forwarded to the handler.
     * @param dbUpdateFunc Throws on error.
     */
    template<
        typename DbFunc,
        typename CompletionHandler,
        typename AppResult = std::invoke_result_t<DbFunc, QueryContext*>,
        typename = std::enable_if_t<
            !std::is_same_v<AppResult, void>
            && !std::is_same_v<AppResult, DBResult>
            && !std::is_same_v<AppResult, DBResultCode>
        >
    >
    void executeUpdate(
        DbFunc dbUpdateFunc,
        CompletionHandler handler,
        const std::string& queryAggregationKey = std::string())
    {
        auto result = std::make_unique<AppResult>();
        auto resultPtr = result.get();
        executeUpdate(
            [dbUpdateFunc = std::move(dbUpdateFunc), resultPtr](auto queryContext) mutable -> void
            {
                *resultPtr = dbUpdateFunc(queryContext);
            },
            [handler = std::move(handler), result = std::move(result)](DBResult dbResult) mutable
            {
                handler(std::move(dbResult), std::move(*result));
            },
            queryAggregationKey);
    }

    /**
     * Convenience overload for executeSelect where DbFunc returns void or throws an exception.
     */
    template<
        typename DbFunc,
        typename =
            std::enable_if_t<std::is_same_v<std::invoke_result_t<DbFunc, QueryContext*>, void>>
    >
    void executeSelect(
        DbFunc dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
    {
        executeSelect(
            [dbUpdateFunc = std::move(dbUpdateFunc)](auto queryContext) -> DBResult
            {
                dbUpdateFunc(queryContext);
                return DBResultCode::ok;
            },
            std::move(completionHandler));
    }

    //---------------------------------------------------------------------------------------------
    // Synchronous operations.

    /**
     * @param script Contain mutiple SQL statements separated with semicolon.
     */
    virtual DBResult execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& script) = 0;

    /**
     * @param queryFunc throws nx::sql::Exception.
     * @throws nx::sql::Exception.
     */
    template<
        typename QueryFunc,
        typename Result = decltype(std::declval<QueryFunc>()((nx::sql::QueryContext*) nullptr))
    >
    Result executeUpdateSync(QueryFunc queryFunc)
    {
        std::promise<nx::sql::DBResult> queryDonePromise;
        std::promise<Result> resultAvailable;
        executeUpdate(
            [&queryFunc, &resultAvailable](nx::sql::QueryContext* queryContext)
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
                return DBResult::success();
            },
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(std::move(dbResult));
            });

        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::sql::DBResultCode::ok)
            throw nx::sql::Exception(dbResult);

        return resultAvailable.get_future().get();
    }

    /**
     * @param script Multiple SQL statements split with semicolon.
     * Throws on failure.
     */
    void execSqlScriptSync(const std::string& script)
    {
        executeUpdateSync([this, &script](QueryContext* queryContext) {
            return execSqlScript(queryContext, script);
        });
    }

    /**
     * @throws nx::sql::Exception.
     */
    void executeSqlSync(const std::string& sqlStatement);

    /**
     * @param queryFunc throws nx::sql::Exception.
     * @throws nx::sql::Exception.
     */
    template<typename QueryFunc>
    typename std::invoke_result_t<QueryFunc, nx::sql::QueryContext*>
        executeSelectSync(QueryFunc queryFunc)
    {
        std::invoke_result_t<QueryFunc, nx::sql::QueryContext*> result;

        nx::utils::promise<nx::sql::DBResult> queryDonePromise;
        executeSelect(
            [&queryFunc, &result](nx::sql::QueryContext* queryContext)
            {
                result = queryFunc(queryContext);
                return nx::sql::DBResultCode::ok;
            },
            [&queryDonePromise](nx::sql::DBResult dbResult)
            {
                queryDonePromise.set_value(dbResult);
            });
        const auto dbResult = queryDonePromise.get_future().get();
        if (dbResult != nx::sql::DBResultCode::ok)
            throw nx::sql::Exception(dbResult);

        return result;
    }

    //---------------------------------------------------------------------------------------------
    // Cursor operations.

    /**
     * Create cursor for record-by-record access to the results of query defined by prepareCursorFunc.
     * @param prepareCursorFunc Invoked to prepare a query. The query is executed after return of
     * this function.
     * @param readRecordFunc Invoked to read the next record from the query prepared with
     * prepareCursorFunc.
     */
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

    /**
     * Executes readRecordFunc previously passed to createCursor within an DB thread.
     * When the end of query result is reached, DBResult::endOfData is reported to the
     * completionHandler.
     * @param id Provided by createCursor.
     * @param completionHandler Invoked with data read by readRecordFunc.
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

    /**
     * @param id Provided by createCursor.
     */
    virtual void removeCursor(QnUuid id) = 0;

    virtual int openCursorCount() const = 0;

    /**
     * See AbstractAsyncSqlQueryExecutor::createCursor.
     * NOTE: The method is public to allow delegation.
     */
    virtual void createCursorImpl(std::unique_ptr<detail::AbstractCursorHandler> cursorHandler) = 0;

    /**
     * See AbstractAsyncSqlQueryExecutor::fetchNextRecordFromCursorImpl.
     * NOTE: The method is public to allow delegation.
     */
    virtual void fetchNextRecordFromCursorImpl(
        std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> task) = 0;
};

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
    virtual void setQueryTimeoutEnabled(bool enabled) override;

    /**
     * Overload for updates with no input data.
     */
    virtual void executeUpdate(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
        const std::string& queryAggregationKey = std::string()) override;

    virtual void executeSelect(
        nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
        nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler) override;

    virtual DBResult execSqlScript(
        nx::sql::QueryContext* const queryContext,
        const std::string& script) override;

    virtual QueryQueueStats queryQueueStatistics() const override;
    virtual QueryStatistics queryStatistics() const override;

    /** Have to introduce this method because we do not use exceptions. */
    bool init();

    const StatisticsCollector& statisticsCollector() const;

    void reserveConnections(int count);

    virtual void createCursorImpl(
        std::unique_ptr<detail::AbstractCursorHandler> cursorHandler) override;

    virtual void fetchNextRecordFromCursorImpl(
        std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> executor) override;

    virtual void removeCursor(QnUuid id) override;

    virtual int openCursorCount() const override;

    using ConnectionFactoryFunc = nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractDbConnection>(
        const ConnectionOptions& connectionOptions)>;

    void setCustomConnectionFactory(std::optional<ConnectionFactoryFunc> func);

private:
    struct CursorProcessorContext
    {
        detail::CursorHandlerPool cursorContextPool;
        std::unique_ptr<detail::BaseQueryExecutor> processingThread;
    };

    const ConnectionOptions m_connectionOptions;
    mutable nx::Mutex m_mutex;
    detail::QueryQueue m_queryQueue;
    StatisticsCollector m_statisticsCollector;
    std::vector<std::unique_ptr<detail::BaseQueryExecutor>> m_dbThreads;
    std::atomic<std::size_t> m_dbThreadsSize{0};
    nx::utils::thread m_dropConnectionThread;
    nx::utils::SyncQueue<std::unique_ptr<detail::BaseQueryExecutor>> m_connectionsToDropQueue;
    bool m_terminated = false;
    std::optional<ConnectionFactoryFunc> m_connectionFactory;

    detail::QueryQueue m_cursorTaskQueue;
    std::vector<std::unique_ptr<CursorProcessorContext>> m_cursorProcessorContexts;

    bool isNewConnectionNeeded() const;

    void openNewConnection(
        const nx::Locker<nx::Mutex>& /*lk*/,
        std::chrono::milliseconds connectDelay = std::chrono::milliseconds::zero());

    void saveOpenedConnection(
        const nx::Locker<nx::Mutex>& lock,
        std::unique_ptr<detail::BaseQueryExecutor> connection);

    std::unique_ptr<detail::BaseQueryExecutor> createNewConnectionThread(
        const ConnectionOptions& connectionOptions,
        detail::QueryQueue* queryQueue);

    void dropExpiredConnectionsThreadFunc();
    void reportQueryCancellation(std::unique_ptr<detail::AbstractExecutor>);
    void onConnectionClosed(detail::BaseQueryExecutor* const executorThreadPtr);

    void dropConnectionAsync(
        const nx::Locker<nx::Mutex>&,
        detail::BaseQueryExecutor* const executorThreadPtr);

    template<typename Executor, typename UpdateFunc, typename CompletionHandler>
    void scheduleQuery(
        const std::string& queryAggregationKey,
        UpdateFunc updateFunc,
        CompletionHandler completionHandler);

    void addCursorProcessingThread(const nx::Locker<nx::Mutex>& lock);
};

} // namespace nx::sql
