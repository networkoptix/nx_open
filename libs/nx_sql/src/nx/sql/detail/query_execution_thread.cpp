// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

namespace nx::sql::detail {

QueryExecutionThread::QueryExecutionThread(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
:
    BaseQueryExecutor(connectionOptions, queryExecutorQueue),
    m_dbConnectionHolder(connectionOptions)
{
}

QueryExecutionThread::QueryExecutionThread(
    const ConnectionOptions& connectionOptions,
    std::unique_ptr<AbstractDbConnection> connection,
    QueryExecutorQueue* const queryExecutorQueue)
    :
    BaseQueryExecutor(connectionOptions, queryExecutorQueue),
    m_dbConnectionHolder(connectionOptions, std::move(connection))
{
}

QueryExecutionThread::~QueryExecutionThread()
{
    if (m_queryExecutionThread.joinable())
    {
        pleaseStop();
        m_queryExecutionThread.join();
    }
    m_dbConnectionHolder.close();
}

void QueryExecutionThread::pleaseStop()
{
    m_terminated = true;
}

void QueryExecutionThread::join()
{
    m_queryExecutionThread.join();
}

ConnectionState QueryExecutionThread::state() const
{
    return m_state;
}

void QueryExecutionThread::setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onClosedHandler = std::move(handler);
}

void QueryExecutionThread::start(std::chrono::milliseconds connectDelay)
{
    m_connectDelay = connectDelay;
    m_queryExecutionThread = std::thread([this]() { queryExecutionThreadMain(); });
}

void QueryExecutionThread::queryExecutionThreadMain()
{
    constexpr const std::chrono::milliseconds kTaskWaitTimeout =
        std::chrono::milliseconds(50);

    auto invokeOnClosedHandlerGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (m_onClosedHandler)
                m_onClosedHandler();
        });

    if (m_connectDelay > std::chrono::milliseconds::zero())
        std::this_thread::sleep_for(m_connectDelay);

    if (!m_dbConnectionHolder.open())
    {
        m_state = ConnectionState::closed;
        return;
    }

    m_state = ConnectionState::opened;

    auto previousActivityTime = std::chrono::steady_clock::now();

    while (!m_terminated && m_state == ConnectionState::opened)
    {
        std::optional<std::unique_ptr<AbstractExecutor>> task =
            queryExecutorQueue()->pop(kTaskWaitTimeout);
        if (!task)
        {
            if (connectionOptions().inactivityTimeout > std::chrono::seconds::zero() &&
                (std::chrono::steady_clock::now() - previousActivityTime >=
                 connectionOptions().inactivityTimeout))
            {
                // Dropping connection by timeout.
                NX_VERBOSE(this, "Closing DB connection by timeout (%1)",
                    connectionOptions().inactivityTimeout);
                closeConnection();
                break;
            }
            continue;
        }

        processTask(std::move(*task));
        if (m_state == ConnectionState::closed)
            break;

        previousActivityTime = std::chrono::steady_clock::now();
    }
}

void QueryExecutionThread::processTask(std::unique_ptr<AbstractExecutor> task)
{
    const auto result = task->execute(m_dbConnectionHolder.dbConnection());
    handleExecutionResult(result);
}

AbstractDbConnection* QueryExecutionThread::connection()
{
    return m_dbConnectionHolder.dbConnection();
}

void QueryExecutionThread::handleExecutionResult(DBResult result)
{
    switch (result)
    {
        case DBResult::ok:
        case DBResult::cancelled:
            m_numberOfFailedRequestsInARow = 0;
            break;

        default:
        {
            ++m_numberOfFailedRequestsInARow;
            if (isDbErrorRecoverable(result))
            {
                NX_DEBUG(this, nx::format("DB query failed with result code %1. Db text %2")
                    .args(result, m_dbConnectionHolder.dbConnection()->lastErrorText()));
            }
            else
            {
                NX_WARNING(this, nx::format("Dropping DB connection due to unrecoverable error %1. Db text %2")
                    .args(result, m_dbConnectionHolder.dbConnection()->lastErrorText()));
                closeConnection();
                break;
            }

            if (m_numberOfFailedRequestsInARow >=
                connectionOptions().maxErrorsInARowBeforeClosingConnection)
            {
                NX_WARNING(this, nx::format("Dropping DB connection due to %1 errors in a row. "
                    "Last error %2. Db text %3")
                    .args(m_numberOfFailedRequestsInARow, result,
                        m_dbConnectionHolder.dbConnection()->lastErrorText()));
                closeConnection();
                break;
            }
        }
    }
}

void QueryExecutionThread::closeConnection()
{
    m_dbConnectionHolder.close();
    m_state = ConnectionState::closed;
}

bool QueryExecutionThread::isDbErrorRecoverable(DBResult dbResult)
{
    switch (dbResult)
    {
        case DBResult::notFound:
        case DBResult::statementError:
        case DBResult::cancelled:
        case DBResult::retryLater:
        case DBResult::uniqueConstraintViolation:
        case DBResult::logicError:
            return true;

        case DBResult::ioError:
        case DBResult::connectionError:
            return false;

        default:
            NX_ASSERT(false);
            return false;
    }
}

} // namespace nx::sql::detail
