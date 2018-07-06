#include "request_executor.h"

#include <QtSql/QSqlError>

#include <nx/utils/time.h>

namespace nx::sql {

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

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutor

UpdateWithoutAnyDataExecutor::UpdateWithoutAnyDataExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
    :
    base_type(std::move(completionHandler)),
    m_dbUpdateFunc(std::move(dbUpdateFunc))
{
}

DBResult UpdateWithoutAnyDataExecutor::doQuery(QueryContext* queryContext)
{
    return invokeDbQueryFunc(m_dbUpdateFunc, queryContext);
}

void UpdateWithoutAnyDataExecutor::reportSuccess()
{
    invokeCompletionHandler(DBResult::ok);
}

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutorNoTran

UpdateWithoutAnyDataExecutorNoTran::UpdateWithoutAnyDataExecutorNoTran(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
    :
    base_type(std::move(completionHandler)),
    m_dbUpdateFunc(std::move(dbUpdateFunc))
{
    setExternalTransaction(nullptr); //< Run without transaction.
}

DBResult UpdateWithoutAnyDataExecutorNoTran::doQuery(QueryContext* queryContext)
{
    return invokeDbQueryFunc(m_dbUpdateFunc, queryContext);
}

void UpdateWithoutAnyDataExecutorNoTran::reportSuccess()
{
    invokeCompletionHandler(DBResult::ok);
}

//-------------------------------------------------------------------------------------------------
// SelectExecutor

SelectExecutor::SelectExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler)
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
        completionHandler(result);
    return result;
}

void SelectExecutor::reportErrorWithoutExecution(DBResult errorCode)
{
    m_completionHandler(errorCode);
}

} // namespace nx::sql
