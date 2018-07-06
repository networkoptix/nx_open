#pragma once

#include <vector>

#include "query_executor.h"

namespace nx::sql::detail {

/**
 * If any query fails, then every query is considered failed and transaction is rolled back.
 */
class NX_SQL_API MultipleQueryExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    MultipleQueryExecutor(
        std::vector<std::unique_ptr<AbstractUpdateExecutor>> queries);

    virtual void reportErrorWithoutExecution(DBResult errorCode) override;

protected:
    virtual DBResult executeQuery(AbstractDbConnection* const connection) override;

private:
    using Queries = std::vector<std::unique_ptr<AbstractUpdateExecutor>>;

    Queries m_queries;

    void reportQueryFailure(
        Queries::iterator begin,
        Queries::iterator end,
        DBResult dbResult);

    std::tuple<DBResult, typename Queries::iterator> executeQueries(
        AbstractDbConnection* const connection,
        Transaction* transaction);
};

} // namespace nx::sql::detail
