#include "request_execution_thread.h"

#include <QtCore/QUuid>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <nx/fusion/serialization/lexical.h>
#include <nx/utils/log/log.h>

#include <utils/common/guard.h>

namespace nx {
namespace db {

DbConnectionHolder::DbConnectionHolder(const ConnectionOptions& connectionOptions):
    m_connectionOptions(connectionOptions)
{
}

DbConnectionHolder::~DbConnectionHolder()
{
    close();
}

const ConnectionOptions& DbConnectionHolder::connectionOptions() const
{
    return m_connectionOptions;
}

bool DbConnectionHolder::open()
{
    // Using guid as a unique connection name.
    m_dbConnection = QSqlDatabase::addDatabase(
        QnLexical::serialized<RdbmsDriverType>(connectionOptions().driverType),
        QUuid::createUuid().toString());
    m_dbConnection.setConnectOptions(connectionOptions().connectOptions);
    m_dbConnection.setDatabaseName(connectionOptions().dbName);
    m_dbConnection.setHostName(connectionOptions().hostName);
    m_dbConnection.setUserName(connectionOptions().userName);
    m_dbConnection.setPassword(connectionOptions().password);
    m_dbConnection.setPort(connectionOptions().port);
    if (!m_dbConnection.open())
    {
        NX_LOG(lit("Failed to establish connection to DB %1 at %2:%3. %4").
            arg(connectionOptions().dbName).arg(connectionOptions().hostName).
            arg(connectionOptions().port).arg(m_dbConnection.lastError().text()),
            cl_logWARNING);
        return false;
    }

    if (!tuneConnection())
    {
        m_dbConnection.close();
        return false;
    }

    return true;
}

QSqlDatabase* DbConnectionHolder::dbConnection()
{
    return &m_dbConnection;
}

void DbConnectionHolder::close()
{
    m_dbConnection.close();
}

bool DbConnectionHolder::tuneConnection()
{
    switch (connectionOptions().driverType)
    {
        case RdbmsDriverType::mysql:
            return tuneMySqlConnection();
        default:
            return true;
    }
}

bool DbConnectionHolder::tuneMySqlConnection()
{
    if (!connectionOptions().encoding.isEmpty())
    {
        QSqlQuery query(m_dbConnection);
        query.prepare(lit("SET NAMES '%1'").arg(connectionOptions().encoding));
        if (!query.exec())
        {
            NX_LOGX(lm("Failed to set connection character set to \"%1\". %2")
                .arg(connectionOptions().encoding).arg(query.lastError().text()),
                cl_logWARNING);
            return false;
        }
    }

    return true;
}

//-------------------------------------------------------------------------------------------------
// DbRequestExecutionThread

DbRequestExecutionThread::DbRequestExecutionThread(
    const ConnectionOptions& connectionOptions,
    QueryExecutorQueue* const queryExecutorQueue)
:
    BaseRequestExecutor(connectionOptions, queryExecutorQueue),
    m_state(ConnectionState::initializing),
    m_terminated(false),
    m_numberOfFailedRequestsInARow(0),
    m_dbConnectionHolder(connectionOptions)
{
}

DbRequestExecutionThread::~DbRequestExecutionThread()
{
    if (m_queryExecutionThread.joinable())
        m_queryExecutionThread.join();
    m_dbConnectionHolder.close();
}

void DbRequestExecutionThread::pleaseStop()
{
    m_terminated = true;
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

    auto invokeOnClosedHandlerGuard = makeScopedGuard(
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
            queryExecutorQueue()->pop(kTaskWaitTimeout);
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
            NX_LOGX(lit("DB query failed with error %1. Db text %2")
                .arg(QnLexical::serialized(result)).arg(m_dbConnectionHolder.dbConnection()->lastError().text()),
                cl_logWARNING);
            ++m_numberOfFailedRequestsInARow;
            if (!isDbErrorRecoverable(result))
            {
                NX_LOGX(lit("Dropping DB connection due to unrecoverable error %1. Db text %2")
                    .arg(QnLexical::serialized(result)).arg(m_dbConnectionHolder.dbConnection()->lastError().text()),
                    cl_logWARNING);
                closeConnection();
                break;
            }
            if (m_numberOfFailedRequestsInARow >= 
                connectionOptions().maxErrorsInARowBeforeClosingConnection)
            {
                NX_LOGX(lit("Dropping DB connection due to %1 errors in a row. Db text %2")
                    .arg(QnLexical::serialized(result)), cl_logWARNING);
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
} // namespace nx
