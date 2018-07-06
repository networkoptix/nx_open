#include "multiple_request_executor.h"

namespace nx::sql {

MultipleRequestExecutor::MultipleRequestExecutor(
    std::vector<std::unique_ptr<AbstractUpdateExecutor>> queries)
    :
    base_type(QueryType::modification),
    m_queries(std::move(queries))
{
}

DBResult MultipleRequestExecutor::executeQuery(QSqlDatabase* const connection)
{
    Transaction transaction(connection);

    Queries::iterator unexecutedQueriesRangeStart = m_queries.begin();

    auto result = transaction.begin();
    if (result != DBResult::ok)
    {
        reportQueryFailure(unexecutedQueriesRangeStart, m_queries.end(), result);
        return result;
    }

    std::tie(result, unexecutedQueriesRangeStart) =
        executeQueries(connection, &transaction);

    if (result != DBResult::ok)
    {
        transaction.rollback();
        reportQueryFailure(unexecutedQueriesRangeStart, m_queries.end(), result);
        return result;
    }

    return transaction.commit();
}

std::tuple<DBResult, typename MultipleRequestExecutor::Queries::iterator>
    MultipleRequestExecutor::executeQueries(
        QSqlDatabase* const connection,
        Transaction* transaction)
{
    DBResult result = DBResult::ok;

    Queries::iterator queryToExecuteIter = m_queries.begin();
    for (queryToExecuteIter = m_queries.begin();
        queryToExecuteIter != m_queries.end();
        ++queryToExecuteIter)
    {
        (*queryToExecuteIter)->setExternalTransaction(transaction);
        result = (*queryToExecuteIter)->execute(connection);
        if (result == DBResult::ok)
            continue;

        // Queries that have been already invoked will report result themselves.
        queryToExecuteIter = std::next(queryToExecuteIter);
        break;
    }

    return std::make_tuple(result, queryToExecuteIter);
}

void MultipleRequestExecutor::reportQueryFailure(
    Queries::iterator begin,
    Queries::iterator end,
    DBResult dbResult)
{
    std::for_each(
        begin, end,
        [dbResult](auto& query) { query->reportErrorWithoutExecution(dbResult); });
}

} // namespace nx::sql
