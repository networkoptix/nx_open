#include "request_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace db {

DbRequestExecutionThread::DbRequestExecutionThread(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
:
    BaseRequestExecutor(connectionOptions, queryExecutorQueue),
    m_state(ConnectionState::initializing),
    m_terminated(false),
    m_numberOfFailedRequestsInARow(0),
    m_dbConnectionHolder(connectionOptions),
    m_queueReaderId(queryExecutorQueue->generateReaderId())
{
}

DbRequestExecutionThread::~DbRequestExecutionThread()
{
    if (m_queryExecutionThread.joinable())
    {
        pleaseStop();
        m_queryExecutionThread.join();
    }
    m_dbConnectionHolder.close();

    queryExecutorQueue()->removeReaderFromTerminatedList(m_queueReaderId);
}

void DbRequestExecutionThread::pleaseStop()
{
    m_terminated = true;
    queryExecutorQueue()->addReaderToTerminatedList(m_queueReaderId);
}

void DbRequestExecutionThread::join()
{
    m_queryExecutionThread.join();
}

ConnectionState DbRequestExecutionThread::state() const
{
    return m_state;
}

void DbRequestExecutionThread::setOnClosedHandler(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onClosedHandler = std::move(handler);
}

void DbRequestExecutionThread::start()
{
    m_queryExecutionThread =
        nx::utils::thread(std::bind(&DbRequestExecutionThread::queryExecutionThreadMain, this));
}

void DbRequestExecutionThread::queryExecutionThreadMain()
{
    constexpr const std::chrono::milliseconds kTaskWaitTimeout = std::chrono::seconds(1);

    auto invokeOnClosedHandlerGuard = makeScopeGuard(
        [onClosedHandler = std::move(m_onClosedHandler)]()
        {
            if (onClosedHandler)
                onClosedHandler();
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
        boost::optional<std::unique_ptr<AbstractExecutor>> task =
            queryExecutorQueue()->pop(kTaskWaitTimeout, m_queueReaderId);
        if (!task)
        {
            if (std::chrono::steady_clock::now() - previousActivityTime >=
                connectionOptions().inactivityTimeout)
            {
                // Dropping connection by timeout.
                NX_LOGX(lm("Closing DB connection by timeout (%1)")
                    .arg(connectionOptions().inactivityTimeout), cl_logDEBUG2);
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

void DbRequestExecutionThread::processTask(std::unique_ptr<AbstractExecutor> task)
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
                NX_LOGX(lm("DB query failed with result code %1. Db text %2")
                    .arg(result).arg(m_dbConnectionHolder.dbConnection()->lastError().text()),
                    cl_logDEBUG1);
            }
            else
            {
                NX_LOGX(lm("Dropping DB connection due to unrecoverable error %1. Db text %2")
                    .arg(result).arg(m_dbConnectionHolder.dbConnection()->lastError().text()),
                    cl_logWARNING);
                closeConnection();
                break;
            }

            if (m_numberOfFailedRequestsInARow >=
                connectionOptions().maxErrorsInARowBeforeClosingConnection)
            {
                NX_LOGX(lm("Dropping DB connection due to %1 errors in a row. Db text %2")
                    .arg(result), cl_logWARNING);
                closeConnection();
                break;
            }
        }
    }
}

void DbRequestExecutionThread::closeConnection()
{
    m_dbConnectionHolder.close();
    m_state = ConnectionState::closed;
}

bool DbRequestExecutionThread::isDbErrorRecoverable(DBResult dbResult)
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

} // namespace db
} // namespace utils
} // namespace nx
