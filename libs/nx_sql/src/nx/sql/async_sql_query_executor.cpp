// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_sql_query_executor.h"

#include <thread>

#include <nx/utils/log/log.h>

#include "detail/query_execution_thread.h"
#include "sql_query_execution_helper.h"
#include "query.h"

namespace nx::sql {

void AbstractAsyncSqlQueryExecutor::executeSqlSync(const std::string& sqlStatement)
{
    executeUpdateSync(
        [&sqlStatement](QueryContext* queryContext)
        {
            auto query = queryContext->connection()->createQuery();
            query->prepare(sqlStatement);
            query->exec();
        });
}

void AbstractAsyncSqlQueryExecutor::executeUpdateWithoutTran(
    nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
{
    executeSelect(std::move(dbSelectFunc), std::move(completionHandler));
}

//-------------------------------------------------------------------------------------------------

static const size_t kDesiredMaxQueuedQueriesPerConnection = 5;
static constexpr std::chrono::minutes kDefaultStatisticsAggregationPeriod = std::chrono::minutes(1);

const int AsyncSqlQueryExecutor::kDefaultQueryPriority = detail::QueryQueue::kDefaultPriority;

AsyncSqlQueryExecutor::AsyncSqlQueryExecutor(
    const ConnectionOptions& connectionOptions)
    :
    m_connectionOptions(connectionOptions),
    m_statisticsCollector(
        kDefaultStatisticsAggregationPeriod,
        m_queryQueue,
        &m_dbThreadPoolSize)
{
    m_dropConnectionThread = nx::utils::thread(
        std::bind(&AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc, this));

    m_queryQueue.setOnItemStayTimeout([this](auto&&... args) {
        reportQueryCancellation(std::forward<decltype(args)>(args)...);
    });

    m_queryQueue.setAggregationLimit(
        m_connectionOptions.maxQueriesAggregatedUnderSingleTransaction);

    if (m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection
            > std::chrono::minutes::zero())
    {
        m_queryQueue.setItemStayTimeout(
            m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection);
    }

    if (m_connectionOptions.driverType == RdbmsDriverType::sqlite)
    {
        // SQLite does not support concurrent DB updates.
        m_queryQueue.setConcurrentModificationQueryLimit(1);
    }
    else if (m_connectionOptions.concurrentModificationQueryLimit)
    {
        m_queryQueue.setConcurrentModificationQueryLimit(
            *m_connectionOptions.concurrentModificationQueryLimit);
    }
}

AsyncSqlQueryExecutor::~AsyncSqlQueryExecutor()
{
    pleaseStopSync();
}

void AsyncSqlQueryExecutor::pleaseStopSync()
{
    if (m_dropConnectionThread.joinable())
    {
        m_connectionsToDropQueue.push(nullptr);
        m_dropConnectionThread.join();
    }

    std::vector<std::unique_ptr<detail::BaseQueryExecutor>> dbThreadPool;
    decltype(m_cursorProcessorContexts) cursorProcessorContexts;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        std::swap(m_dbThreads, dbThreadPool);
        m_dbThreadPoolSize = 0;

        std::swap(m_cursorProcessorContexts, cursorProcessorContexts);
        m_terminated = true;
    }

    for (auto& dbConnection: dbThreadPool)
        dbConnection->pleaseStop();
    dbThreadPool.clear();

    for (auto& context: cursorProcessorContexts)
        context->processingThread->pleaseStop();
    cursorProcessorContexts.clear();
}

const ConnectionOptions& AsyncSqlQueryExecutor::connectionOptions() const
{
    return m_connectionOptions;
}

void AsyncSqlQueryExecutor::executeUpdate(
    nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
    const std::string& queryAggregationKey)
{
    scheduleQuery<detail::UpdateWithoutAnyDataExecutor>(
        queryAggregationKey,
        std::move(dbUpdateFunc),
        std::move(completionHandler));
}

void AsyncSqlQueryExecutor::executeSelect(
    nx::utils::MoveOnlyFunc<DBResult(nx::sql::QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
{
    scheduleQuery<detail::SelectExecutor>(
        std::string(),
        std::move(dbSelectFunc),
        std::move(completionHandler));
}

DBResult AsyncSqlQueryExecutor::execSqlScript(
    nx::sql::QueryContext* const queryContext,
    const std::string& script)
{
    nx::sql::SqlQueryExecutionHelper::execSQLScript(queryContext, script);
    return DBResultCode::ok;
}

bool AsyncSqlQueryExecutor::init()
{
    // Opening a single connection thread and waiting for its initialization result.
    auto executorThread = createNewConnectionThread(m_connectionOptions, &m_queryQueue);
    executorThread->start();

    // Waiting for connection to change state.
    for (;;)
    {
        switch (executorThread->state())
        {
            case detail::ConnectionState::initializing:
                // TODO: #akolesnikov Replace with a "state changed" event from the thread.
                // But, delay is not a big problem since connection to a DB is not a quick thing.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;

            case detail::ConnectionState::opened:
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                saveOpenedConnection(lock, std::move(executorThread));
                return true;
            }

            case detail::ConnectionState::closed:
            default:
                return false;
        }
    }
}

const StatisticsCollector& AsyncSqlQueryExecutor::statisticsCollector() const
{
    return m_statisticsCollector;
}

void AsyncSqlQueryExecutor::reserveConnections(int count)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    for (int i = 0; i < count; ++i)
        openNewConnection(lock);
}

QueryQueueStats AsyncSqlQueryExecutor::queryQueueStatistics() const
{
    return m_queryQueue.stats();
}

QueryStatistics AsyncSqlQueryExecutor::queryStatistics() const
{
    return m_statisticsCollector.getQueryStatistics();
}

void AsyncSqlQueryExecutor::createCursorImpl(
    std::unique_ptr<detail::AbstractCursorHandler> cursorHandler)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_cursorProcessorContexts.empty())
            addCursorProcessingThread(lock);
    }

    auto cursorCreator = std::make_unique<detail::CursorCreator>(
        &m_cursorProcessorContexts.front()->cursorContextPool,
        std::move(cursorHandler));
    m_cursorTaskQueue.push(std::move(cursorCreator));
}

void AsyncSqlQueryExecutor::fetchNextRecordFromCursorImpl(
    std::unique_ptr<detail::AbstractFetchNextRecordFromCursorTask> task)
{
    auto executor = std::make_unique<detail::FetchCursorDataExecutor>(
        &m_cursorProcessorContexts.front()->cursorContextPool,
        std::move(task));

    m_cursorTaskQueue.push(std::move(executor));
}

void AsyncSqlQueryExecutor::removeCursor(QnUuid id)
{
    m_cursorProcessorContexts.front()->cursorContextPool.markCursorForDeletion(id);
    auto task = std::make_unique<detail::CleanUpDroppedCursorsExecutor>(
        &m_cursorProcessorContexts.front()->cursorContextPool);
    m_cursorTaskQueue.push(std::move(task));
}

int AsyncSqlQueryExecutor::openCursorCount() const
{
    return std::accumulate(
        m_cursorProcessorContexts.begin(), m_cursorProcessorContexts.end(),
        0,
        [](int sum, const std::unique_ptr<CursorProcessorContext>& element) -> int
        {
            return sum + (int) element->cursorContextPool.cursorCount();
        });
}

void AsyncSqlQueryExecutor::setCustomConnectionFactory(
    std::optional<ConnectionFactoryFunc> func)
{
    m_connectionFactory = std::move(func);
}

void AsyncSqlQueryExecutor::setConcurrentModificationQueryLimit(int value)
{
    m_queryQueue.setConcurrentModificationQueryLimit(value);
}

void AsyncSqlQueryExecutor::setQueryTimeoutEnabled(bool enabled)
{
    m_queryQueue.setItemStayTimeout(enabled
        ? std::make_optional(m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection)
        : std::nullopt);
}

void AsyncSqlQueryExecutor::setQueryPriority(
    QueryType queryType,
    int newPriority)
{
    m_queryQueue.setQueryPriority(queryType, newPriority);
}

bool AsyncSqlQueryExecutor::isNewConnectionNeeded() const
{
    // TODO: #akolesnikov Check for non-busy threads.

    const auto effectiveDBConnectionCount = m_dbThreadPoolSize.load();
    const auto queueSize = m_queryQueue.pendingQueryCount();
    const auto maxDesiredQueueSize =
        effectiveDBConnectionCount * kDesiredMaxQueuedQueriesPerConnection;
    if (queueSize < maxDesiredQueueSize)
        return false; //< Task number is not too high.
    if (effectiveDBConnectionCount >= static_cast<size_t>(m_connectionOptions.maxConnectionCount))
        return false; //< Pool size is already at maximum.

    return true;
}

void AsyncSqlQueryExecutor::openNewConnection(
    const nx::Locker<nx::Mutex>& lock,
    std::chrono::milliseconds connectDelay)
{
    auto executorThread = createNewConnectionThread(m_connectionOptions, &m_queryQueue);
    auto executorThreadPtr = executorThread.get();
    saveOpenedConnection(lock, std::move(executorThread));
    executorThreadPtr->start(connectDelay);
}

void AsyncSqlQueryExecutor::saveOpenedConnection(
    const nx::Locker<nx::Mutex>& /*lock*/,
    std::unique_ptr<detail::BaseQueryExecutor> connection)
{
    connection->setOnClosedHandler(
        std::bind(&AsyncSqlQueryExecutor::onConnectionClosed, this, connection.get()));

    m_dbThreads.push_back(std::move(connection));
    ++m_dbThreadPoolSize;
}

std::unique_ptr<detail::BaseQueryExecutor> AsyncSqlQueryExecutor::createNewConnectionThread(
    const ConnectionOptions& connectionOptions,
    detail::QueryQueue* queryQueue)
{
    if (m_connectionFactory)
    {
        return std::make_unique<detail::QueryExecutionThread>(
            connectionOptions,
            (*m_connectionFactory)(connectionOptions),
            queryQueue);
    }
    else
    {
        return std::make_unique<detail::QueryExecutionThread>(connectionOptions, queryQueue);
    }
}

void AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc()
{
    for (;;)
    {
        std::unique_ptr<detail::BaseQueryExecutor> dbConnection =
            m_connectionsToDropQueue.pop();
        if (!dbConnection)
            return; //null is used as a termination mark

        dbConnection->join();
        dbConnection.reset();
    }
}

void AsyncSqlQueryExecutor::reportQueryCancellation(
    std::unique_ptr<detail::AbstractExecutor> expiredQuery)
{
    // We are in a random db request execution thread.
    expiredQuery->reportErrorWithoutExecution(DBResultCode::cancelled);
}

void AsyncSqlQueryExecutor::onConnectionClosed(
    detail::BaseQueryExecutor* const executorThreadPtr)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    dropConnectionAsync(lk, executorThreadPtr);
    if (m_dbThreads.empty() && !m_terminated)
        // Attempting to open a connection only after a delay.
        openNewConnection(lk, m_connectionOptions.reconnectAfterFailureDelay);
}

void AsyncSqlQueryExecutor::dropConnectionAsync(
    const nx::Locker<nx::Mutex>& /*lk*/,
    detail::BaseQueryExecutor* const executorThreadPtr)
{
    auto it = std::find_if(
        m_dbThreads.begin(),
        m_dbThreads.end(),
        [executorThreadPtr](std::unique_ptr<detail::BaseQueryExecutor>& val)
        {
            return val.get() == executorThreadPtr;
        });

    if (it == m_dbThreads.end())
        return; //< This can happen during AsyncSqlQueryExecutor destruction.
    m_connectionsToDropQueue.push(std::move(*it));

    m_dbThreads.erase(it);
    --m_dbThreadPoolSize;
}

template<typename Executor, typename UpdateFunc, typename CompletionHandler>
void AsyncSqlQueryExecutor::scheduleQuery(
    const std::string& queryAggregationKey,
    UpdateFunc updateFunc,
    CompletionHandler completionHandler)
{
    if (isNewConnectionNeeded())
    {
        NX_MUTEX_LOCKER lk(&m_mutex);

        if (isNewConnectionNeeded())
            openNewConnection(lk);
    }

    auto executor = std::make_unique<Executor>(
        std::move(updateFunc),
        std::move(completionHandler),
        queryAggregationKey);

    executor->setStatisticsCollector(&m_statisticsCollector);
    m_queryQueue.push(std::move(executor));
}

void AsyncSqlQueryExecutor::addCursorProcessingThread(const nx::Locker<nx::Mutex>& /*lock*/)
{
    m_cursorProcessorContexts.push_back(std::make_unique<CursorProcessorContext>());
    // Disabling inactivity timer.
    auto connectionOptions = m_connectionOptions;
    connectionOptions.inactivityTimeout = std::chrono::seconds::zero();
    m_cursorProcessorContexts.back()->processingThread =
        createNewConnectionThread(connectionOptions, &m_cursorTaskQueue);
    m_cursorProcessorContexts.back()->processingThread->start();
}

} // namespace nx::sql
