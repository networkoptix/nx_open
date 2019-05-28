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
    m_state(ConnectionState::initializing),
    m_terminated(false),
    m_numberOfFailedRequestsInARow(0),
    m_dbConnectionHolder(connectionOptions)
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

void QueryExecutionThread::start()
{
    m_queryExecutionThread =
        nx::utils::thread(std::bind(&QueryExecutionThread::queryExecutionThreadMain, this));
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
                NX_VERBOSE(this, lm("Closing DB connection by timeout (%1)")
                    .arg(connectionOptions().inactivityTimeout));
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
                NX_DEBUG(this, lm("DB query failed with result code %1. Db text %2")
                    .args(result, m_dbConnectionHolder.dbConnection()->lastErrorText()));
            }
            else
            {
                NX_WARNING(this, lm("Dropping DB connection due to unrecoverable error %1. Db text %2")
                    .args(result, m_dbConnectionHolder.dbConnection()->lastErrorText()));
                closeConnection();
                break;
            }

            if (m_numberOfFailedRequestsInARow >=
                connectionOptions().maxErrorsInARowBeforeClosingConnection)
            {
                NX_WARNING(this, lm("Dropping DB connection due to %1 errors in a row. "
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
