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
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(m_dbThreadPool, dbThreadPool);
        m_terminated = true;
    }

    for (auto& dbConnection: dbThreadPool)
        dbConnection->pleaseStop();
    dbThreadPool.clear();
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
    QnMutexLocker lk(&m_mutex);
    openNewConnection(lk);
    // TODO: Waiting for connection to change state

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
    QnMutexLocker lock(&m_mutex);
    return m_queryQueue.size();
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

void AsyncSqlQueryExecutor::openNewConnection(const QnMutexLockerBase& /*lk*/)
{
    auto executorThread = RequestExecutorFactory::create(
        m_connectionOptions,
        &m_queryQueue);
    auto executorThreadPtr = executorThread.get();

    m_dbThreadPool.push_back(std::move(executorThread));

    executorThreadPtr->setOnClosedHandler(
        std::bind(&AsyncSqlQueryExecutor::onConnectionClosed, this, executorThreadPtr));
    executorThreadPtr->start();
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

} // namespace db
} // namespace utils
} // namespace nx
