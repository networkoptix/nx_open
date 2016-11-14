/**********************************************************
* 7 may 2015
* a.kolesnikov
***********************************************************/

#include "async_sql_query_executor.h"

#include <thread>

#include <nx/utils/log/log.h>

#include "request_executor_factory.h"

namespace nx {
namespace db {

static const size_t kDesiredMaxQueuedQueriesPerConnection = 5;

AsyncSqlQueryExecutor::AsyncSqlQueryExecutor(
    const ConnectionOptions& connectionOptions)
    :
    m_connectionOptions(connectionOptions),
    m_terminated(false)
{
    m_dropConnectionThread =
        nx::utils::thread(
            std::bind(&AsyncSqlQueryExecutor::dropExpiredConnectionsThreadFunc, this));

    using namespace std::placeholders;
    if (m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection
            > std::chrono::minutes::zero())
    {
        m_requestQueue.enableItemStayTimeoutEvent(
            m_connectionOptions.maxPeriodQueryWaitsForAvailableConnection,
            std::bind(&AsyncSqlQueryExecutor::reportQueryCancellation, this, _1));
    }
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

bool AsyncSqlQueryExecutor::init()
{
    QnMutexLocker lk(&m_mutex);
    openNewConnection(lk);
    // TODO: Waiting for connection to change state

    return true;
}

bool AsyncSqlQueryExecutor::isNewConnectionNeeded(const QnMutexLockerBase& /*lk*/) const
{
    const auto effectiveDBConnectionCount = m_dbThreadPool.size();
    const auto queueSize = static_cast< size_t >(m_requestQueue.size());
    const auto maxDesiredQueueSize =
        effectiveDBConnectionCount * kDesiredMaxQueuedQueriesPerConnection;
    if (queueSize < maxDesiredQueueSize)
        return false;    //< Task number is not too high.
    if (effectiveDBConnectionCount >= m_connectionOptions.maxConnectionCount)
        return false;    //< Pool size is already at maximum.

    return true;
}

void AsyncSqlQueryExecutor::openNewConnection(const QnMutexLockerBase& /*lk*/)
{
    auto executorThread = RequestExecutorFactory::create(
        m_connectionOptions,
        &m_requestQueue);
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
        std::unique_ptr<BaseRequestExecutor> dbConnection;
        m_connectionsToDropQueue.pop(dbConnection);
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
} // namespace nx
