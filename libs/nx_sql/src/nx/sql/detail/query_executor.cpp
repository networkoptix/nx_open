// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "query_executor.h"

#include <QtSql/QSqlError>

#include <nx/utils/time.h>

namespace nx::sql::detail {

//-------------------------------------------------------------------------------------------------
// BaseExecutor

BaseExecutor::BaseExecutor(
    QueryType queryType,
    const std::string& queryAggregationKey)
    :
    m_statisticsCollector(nullptr),
    m_creationTime(nx::utils::monotonicTime()),
    m_queryExecuted(false),
    m_queryType(queryType),
    m_queryAggregationKey(queryAggregationKey),
    m_completionHandlerTelemetrySpan(nx::telemetry::Span::activeSpan())
{
    if (m_completionHandlerTelemetrySpan.isValid())
    {
        m_telemetrySpan = nx::telemetry::Span("db", m_completionHandlerTelemetrySpan);
        m_telemetrySpan.setAttribute("query-type", nx::reflect::enumeration::toString(queryType));
    }
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
        m_statisticsCollector->recordQueryExecutionTask(m_queryStatistics);

    if (m_onBeforeDestructionHandler)
        m_onBeforeDestructionHandler();
}

DBResult BaseExecutor::execute(AbstractDbConnection* const connection)
{
    using namespace std::chrono;

    const auto executionStartTime = nx::utils::monotonicTime();

    m_queryStatistics.waitForExecutionDuration =
        duration_cast<milliseconds>(executionStartTime - m_creationTime);
    m_queryExecuted = true;

    m_queryStatistics.result = executeQuery(connection);

    m_queryStatistics.executionDuration =
        duration_cast<milliseconds>(nx::utils::monotonicTime() - executionStartTime);

    m_telemetrySpan.setStatus(
        m_queryStatistics.result->ok()
            ? nx::telemetry::Span::Status::ok
            : nx::telemetry::Span::Status::error,
        m_queryStatistics.result->toString());

    m_telemetrySpan.end();

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

std::string BaseExecutor::aggregationKey() const
{
    return m_queryAggregationKey;
}

void BaseExecutor::setStatisticsCollector(StatisticsCollector* statisticsCollector)
{
    m_statisticsCollector = statisticsCollector;
}

//-------------------------------------------------------------------------------------------------
// UpdateWithoutAnyDataExecutor

UpdateWithoutAnyDataExecutor::UpdateWithoutAnyDataExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext* const)> dbUpdateFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
    const std::string& queryAggregationKey)
    :
    base_type(
        std::move(completionHandler),
        queryAggregationKey),
    m_dbUpdateFunc(std::move(dbUpdateFunc))
{
}

DBResult UpdateWithoutAnyDataExecutor::doQuery(QueryContext* queryContext)
{
    return invokeDbQueryFunc(m_dbUpdateFunc, queryContext);
}

void UpdateWithoutAnyDataExecutor::reportSuccess()
{
    invokeCompletionHandler(DBResultCode::ok);
}

//-------------------------------------------------------------------------------------------------
// SelectExecutor

SelectExecutor::SelectExecutor(
    nx::utils::MoveOnlyFunc<DBResult(QueryContext*)> dbSelectFunc,
    nx::utils::MoveOnlyFunc<void(DBResult)> completionHandler,
    const std::string& queryAggregationKey)
    :
    base_type(QueryType::lookup, queryAggregationKey),
    m_dbSelectFunc(std::move(dbSelectFunc)),
    m_completionHandler(std::move(completionHandler))
{
}

DBResult SelectExecutor::executeQuery(AbstractDbConnection* const connection)
{
    auto completionHandler = std::move(m_completionHandler);
    QueryContext queryContext(connection, nullptr);
    auto result = invokeDbQueryFunc(m_dbSelectFunc, &queryContext);
    if (completionHandler)
    {
        auto spanScope = m_completionHandlerTelemetrySpan.activate();
        completionHandler(result);
    }
    return result;
}

void SelectExecutor::reportErrorWithoutExecution(DBResult errorCode)
{
    auto spanScope = m_completionHandlerTelemetrySpan.activate();
    m_completionHandler(errorCode);
}

void SelectExecutor::setExternalTransaction(Transaction* /*transaction*/)
{
    // Ignoring since SelectExecutor::executeQuery always uses nullptr as a transaction.
}

} // namespace nx::sql::detail
