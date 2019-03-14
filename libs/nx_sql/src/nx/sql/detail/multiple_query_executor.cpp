#include "multiple_query_executor.h"

namespace nx::sql::detail {

MultipleQueryExecutor::MultipleQueryExecutor(
    std::vector<std::unique_ptr<AbstractExecutor>> queries)
    :
    base_type(QueryType::modification, /*No aggregation*/ std::string()),
    m_queries(std::move(queries))
{
}

void MultipleQueryExecutor::reportErrorWithoutExecution(DBResult errorCode)
{
    reportQueryFailure(m_queries.begin(), m_queries.end(), errorCode);
}

void MultipleQueryExecutor::setExternalTransaction(Transaction* /*transaction*/)
{
}

DBResult MultipleQueryExecutor::executeQuery(AbstractDbConnection* const connection)
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

std::tuple<DBResult, typename MultipleQueryExecutor::Queries::iterator>
    MultipleQueryExecutor::executeQueries(
        AbstractDbConnection* const connection,
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

void MultipleQueryExecutor::reportQueryFailure(
    Queries::iterator begin,
    Queries::iterator end,
    DBResult dbResult)
{
    std::for_each(
        begin, end,
        [dbResult](auto& query) { query->reportErrorWithoutExecution(dbResult); });
}

} // namespace nx::sql::detail
