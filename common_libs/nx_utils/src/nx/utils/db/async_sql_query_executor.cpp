#include "async_sql_query_executor.h"

#include <thread>

#include <nx/utils/log/log.h>

#include "request_executor_factory.h"
#include "sql_query_execution_helper.h"
#include "query.h"

namespace nx {
namespace utils {
namespace db {

void AbstractAsyncSqlQueryExecutor::executeSqlSync(QByteArray sqlStatement)
{
    executeUpdateQuerySync(
        [&sqlStatement](QueryContext* queryContext)
        {
            SqlQuery query(*queryContext->connection());
            query.prepare(sqlStatement);
            query.exec();
        });
}

//-------------------------------------------------------------------------------------------------

static const size_t kDesiredMaxQueuedQueriesPerConnection = 5;

AsyncSqlQueryExecutor::AsyncSqlQueryExecutor(
    const ConnectionOptions& connectionOptions)
    :
    m_connectionOptions(connectionOptions),
    m_terminated(false),
    m_statisticsCollector(nullptr)
{
    m_dropConnectionThread =
        nx::utils::thread(
            std::bind(&AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc, this));

    using namespace std::placeholders;
    if (m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection
            > std::chrono::minutes::zero())
    {
        m_queryQueue.enableItemStayTimeoutEvent(
            m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection,
            std::bind(&AsyncSqlQueryExecutor::reportQueryCancellation, this, _1));
    }

    if (m_connectionOptions.driverType == RdbmsDriverType::sqlite)
        m_queryQueue.setConcurrentModificationQueryLimit(1);
}

AsyncSqlQueryExecutor::~AsyncSqlQueryExecutor()
{
    m_connectionsToDropQueue.push(nullptr);
    m_dropConnectionThread.join();

    std::vector<std::unique_ptr<BaseRequestExecutor>> dbThreadPool;
    decltype(m_cursorProcessorContexts) cursorProcessorContexts;
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(m_dbThreadPool, dbThreadPool);
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
    nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler)
{
    scheduleQuery<UpdateWithoutAnyDataExecutor>(
        std::move(dbUpdateFunc),
        std::move(completionHandler));
}

void AsyncSqlQueryExecutor::executeUpdateWithoutTran(
    nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler)
{
    scheduleQuery<UpdateWithoutAnyDataExecutorNoTran>(
        std::move(dbUpdateFunc),
        std::move(completionHandler));
}

void AsyncSqlQueryExecutor::executeSelect(
    nx::utils::MoveOnlyFunc<DBResult(nx::utils::db::QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(nx::utils::db::QueryContext*, DBResult)> completionHandler)
{
    scheduleQuery<SelectExecutor>(
        std::move(dbSelectFunc),
        std::move(completionHandler));
}

DBResult AsyncSqlQueryExecutor::execSqlScriptSync(
    const QByteArray& script,
    nx::utils::db::QueryContext* const queryContext)
{
    return nx::utils::db::SqlQueryExecutionHelper::execSQLScript(script, *queryContext->connection())
        ? DBResult::ok
        : DBResult::ioError;
}

bool AsyncSqlQueryExecutor::init()
{
    {
        QnMutexLocker lk(&m_mutex);
        openNewConnection(lk);
    }

    // Waiting for connection to change state.
    for (;;)
    {
        ConnectionState connectionState = ConnectionState::initializing;
        {
            QnMutexLocker lk(&m_mutex);
            connectionState = m_dbThreadPool.empty()
                ? ConnectionState::closed //< Connection has been closed due to some problem.
                : m_dbThreadPool.front()->state();
        }

        switch (connectionState)
        {
            case ConnectionState::initializing:
                // TODO: #ak Replace with a "state changed" event from the thread.
                // But, delay is not a big problem because connection to a DB is not a quick thing.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;

            case ConnectionState::opened:
                break;

            case ConnectionState::closed:
            default:
                return false;
        }

        break;
    }

    return true;
}

void AsyncSqlQueryExecutor::setStatisticsCollector(
    StatisticsCollector* statisticsCollector)
{
    m_statisticsCollector = statisticsCollector;
}

void AsyncSqlQueryExecutor::reserveConnections(int count)
{
    QnMutexLocker lock(&m_mutex);
    for (int i = 0; i < count; ++i)
        openNewConnection(lock);
}

std::size_t AsyncSqlQueryExecutor::pendingQueryCount() const
{
    return m_queryQueue.size();
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

void AsyncSqlQueryExecutor::setConcurrentModificationQueryLimit(int value)
{
    m_queryQueue.setConcurrentModificationQueryLimit(value);
}

bool AsyncSqlQueryExecutor::isNewConnectionNeeded(const QnMutexLockerBase& /*lk*/) const
{
    // TODO: #ak Check for non-busy threads.

    const auto effectiveDBConnectionCount = m_dbThreadPool.size();
    const auto queueSize = static_cast< size_t >(m_queryQueue.size());
    const auto maxDesiredQueueSize =
        effectiveDBConnectionCount * kDesiredMaxQueuedQueriesPerConnection;
    if (queueSize < maxDesiredQueueSize)
        return false; //< Task number is not too high.
    if (effectiveDBConnectionCount >= static_cast<size_t>(m_connectionOptions.maxConnectionCount))
        return false; //< Pool size is already at maximum.

    return true;
}

void AsyncSqlQueryExecutor::openNewConnection(const QnMutexLockerBase& lock)
{
    auto executorThread = createNewConnectionThread(lock, &m_queryQueue);
    m_dbThreadPool.push_back(std::move(executorThread));
}

std::unique_ptr<BaseRequestExecutor> AsyncSqlQueryExecutor::createNewConnectionThread(
    const QnMutexLockerBase& lock,
    detail::QueryQueue* const queryQueue)
{
    return createNewConnectionThread(lock, m_connectionOptions, queryQueue);
}

std::unique_ptr<BaseRequestExecutor> AsyncSqlQueryExecutor::createNewConnectionThread(
    const QnMutexLockerBase& /*lock*/,
    const ConnectionOptions& connectionOptions,
    detail::QueryQueue* const queryQueue)
{
    auto executorThread = RequestExecutorFactory::instance().create(
        connectionOptions,
        queryQueue);

    executorThread->setOnClosedHandler(
        std::bind(&AsyncSqlQueryExecutor::onConnectionClosed, this, executorThread.get()));
    executorThread->start();

    return executorThread;
}

void AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc()
{
    for (;;)
    {
        std::unique_ptr<BaseRequestExecutor> dbConnection = m_connectionsToDropQueue.pop();
        if (!dbConnection)
            return; //null is used as a termination mark

        dbConnection->join();
        dbConnection.reset();
    }
}

void AsyncSqlQueryExecutor::reportQueryCancellation(
    std::unique_ptr<AbstractExecutor> expiredQuery)
{
    // We are in a random db request execution thread.
    expiredQuery->reportErrorWithoutExecution(DBResult::cancelled);
}

void AsyncSqlQueryExecutor::onConnectionClosed(
    BaseRequestExecutor* const executorThreadPtr)
{
    QnMutexLocker lk(&m_mutex);
    dropConnectionAsync(lk, executorThreadPtr);
    if (m_dbThreadPool.empty() && !m_terminated)
        openNewConnection(lk);
}

void AsyncSqlQueryExecutor::dropConnectionAsync(
    const QnMutexLockerBase& /*lk*/,
    BaseRequestExecutor* const executorThreadPtr)
{
    auto it = std::find_if(
        m_dbThreadPool.begin(),
        m_dbThreadPool.end(),
        [executorThreadPtr](std::unique_ptr<BaseRequestExecutor>& val)
        {
            return val.get() == executorThreadPtr;
        });

    if (it == m_dbThreadPool.end())
        return; //< This can happen during AsyncSqlQueryExecutor destruction.
    m_connectionsToDropQueue.push(std::move(*it));
    m_dbThreadPool.erase(it);
}

void AsyncSqlQueryExecutor::addCursorProcessingThread(const QnMutexLockerBase& lock)
{
    m_cursorProcessorContexts.push_back(std::make_unique<CursorProcessorContext>());
    // Disabling inactivity timer.
    auto connectionOptions = m_connectionOptions;
    connectionOptions.inactivityTimeout = std::chrono::seconds::zero();
    m_cursorProcessorContexts.back()->processingThread =
        createNewConnectionThread(lock, connectionOptions, &m_cursorTaskQueue);
}

} // namespace db
} // namespace utils
} // namespace nx
