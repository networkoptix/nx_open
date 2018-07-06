#pragma once

#include <vector>

#include "request_executor.h"

namespace nx::sql::detail {

class MultipleRequestExecutor:
    public BaseExecutor
{
    using base_type = BaseExecutor;

public:
    MultipleRequestExecutor(
        std::vector<std::unique_ptr<AbstractUpdateExecutor>> queries);

protected:
    virtual DBResult executeQuery(QSqlDatabase* const connection) override;

private:
    using Queries = std::vector<std::unique_ptr<AbstractUpdateExecutor>>;

    Queries m_queries;

    void reportQueryFailure(
        Queries::iterator begin,
        Queries::iterator end,
        DBResult dbResult);

    std::tuple<DBResult, typename Queries::iterator> executeQueries(
        QSqlDatabase* const connection,
        Transaction* transaction);
};

} // namespace nx::sql::detail
