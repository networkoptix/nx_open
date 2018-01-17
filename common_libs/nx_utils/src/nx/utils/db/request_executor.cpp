#include "request_executor.h"

#include <QtSql/QSqlError>

#include <nx/utils/time.h>

namespace nx {
namespace utils {
namespace db {

//-------------------------------------------------------------------------------------------------
// BaseExecutor

BaseExecutor::BaseExecutor(QueryType queryType):
    m_statisticsCollector(nullptr),
    m_creationTime(nx::utils::monotonicTime()),
    m_queryExecuted(false),
    m_queryType(queryType)
{
}

BaseExecutor::~BaseExecutor()
{
    using namespace std::chrono;

    if (!m_queryExecuted)
    {
        m_queryStatistics.waitForExecutionDuration =
            duration_cast<milliseconds>(nx::utils::monotonicTime() - m_creationTime);
    }

    if (m_statisticsCollector)
        m_statisticsCollector->recordQuery(m_queryStatistics);

    if (m_onBeforeDestructionHandler)
        m_onBeforeDestructionHandler();
}

DBResult BaseExecutor::execute(QSqlDatabase* const connection)
{
    using namespace std::chrono;

    const auto executionStartTime = nx::utils::monotonicTime();

    m_queryStatistics.waitForExecutionDuration =
        duration_cast<milliseconds>(executionStartTime - m_creationTime);
    m_queryExecuted = true;

    m_queryStatistics.result = executeQuery(connection);

    m_queryStatistics.executionDuration =
        duration_cast<milliseconds>(nx::utils::monotonicTime() - executionStartTime);
    return *m_queryStatistics.result;
}

QueryType BaseExecutor::queryType() const
{
    return m_queryType;
}

void BaseExecutor::setOnBeforeDestruction(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_onBeforeDestructionHandler = std::move(handler);
}

void BaseExecutor::setStatisticsCollector(StatisticsCollector* statisticsCollector)
{
    m_statisticsCollector = statisticsCollector;
}

DBResult BaseExecutor::detailResultCode(
    QSqlDatabase* const connection,
    DBResult result) const
{
    if (result != DBResult::ioError)
        return result;
    switch (connection->lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;
        case QSqlError::ConnectionError:
            return DBResult::connectionError;
        default:
            return result;
    }
}

DBResult BaseExecutor::lastDBError(QSqlDatabase* const connection) const
{
    switch (connection->lastError().type())
    {
        case QSqlError::StatementError:
            return DBResult::statementError;
        case QSqlError::ConnectionError:
            return DBResult::connectionError;

        case QSqlError::NoError:    //Qt not always sets error code correctly
        case QSqlError::TransactionError:
        case QSqlError::UnknownError:
        default:
            return DBResult::ioError;
    }
}

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutor

UpdateWithoutAnyDataExecutor::UpdateWithoutAnyDataExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
    base_type(QueryType::modification),
    m_dbUpdateFunc(std::move(dbUpdateFunc)),
    m_completionHandler(std::move(completionHandler))
{
}

DBResult UpdateWithoutAnyDataExecutor::executeQuery(QSqlDatabase* const connection)
{
    Transaction transaction(connection);
    QueryContext queryContext(connection, &transaction);

    auto completionHandler = std::move(m_completionHandler);
    auto result = transaction.begin();
    if (result != DBResult::ok)
    {
        result = lastDBError(connection);
        completionHandler(&queryContext, result);
        return result;
    }

    result = invokeDbQueryFunc(m_dbUpdateFunc, &queryContext);
    if (result != DBResult::ok)
    {
        result = detailResultCode(connection, result);
        transaction.rollback();
        completionHandler(&queryContext, result);
        return result;
    }

    result = transaction.commit();
    if (result != DBResult::ok)
    {
        result = lastDBError(connection);
        connection->rollback();
        completionHandler(&queryContext, result);
        return result;
    }

    completionHandler(&queryContext, DBResult::ok);
    return DBResult::ok;
}

void UpdateWithoutAnyDataExecutor::reportErrorWithoutExecution(DBResult errorCode)
{
    m_completionHandler(nullptr, errorCode);
}

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutorNoTran

UpdateWithoutAnyDataExecutorNoTran::UpdateWithoutAnyDataExecutorNoTran(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
    base_type(QueryType::modification),
    m_dbUpdateFunc(std::move(dbUpdateFunc)),
    m_completionHandler(std::move(completionHandler))
{
}

DBResult UpdateWithoutAnyDataExecutorNoTran::executeQuery(QSqlDatabase* const connection)
{
    auto completionHandler = std::move(m_completionHandler);
    QueryContext queryContext(connection, nullptr);
    auto result = invokeDbQueryFunc(m_dbUpdateFunc, &queryContext);
    result = detailResultCode(connection, result);
    completionHandler(&queryContext, result);
    return result;
}

void UpdateWithoutAnyDataExecutorNoTran::reportErrorWithoutExecution(DBResult errorCode)
{
    m_completionHandler(nullptr, errorCode);
}

//-------------------------------------------------------------------------------------------------
// SelectExecutor

SelectExecutor::SelectExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(QueryContext*, DBResult)> completionHandler)
    :
    base_type(QueryType::lookup),
    m_dbSelectFunc(std::move(dbSelectFunc)),
    m_completionHandler(std::move(completionHandler))
{
}

DBResult SelectExecutor::executeQuery(QSqlDatabase* const connection)
{
    auto completionHandler = std::move(m_completionHandler);
    QueryContext queryContext(connection, nullptr);
    auto result = invokeDbQueryFunc(m_dbSelectFunc, &queryContext);
    result = detailResultCode(connection, result);
    if (completionHandler)
        completionHandler(&queryContext, result);
    return result;
}

void SelectExecutor::reportErrorWithoutExecution(DBResult errorCode)
{
    m_completionHandler(nullptr, errorCode);
}

} // namespace db
} // namespace utils
} // namespace nx
