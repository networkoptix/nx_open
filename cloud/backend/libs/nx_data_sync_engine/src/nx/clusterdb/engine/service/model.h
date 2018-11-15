#pragma once

#include <memory>

#include <nx/sql/async_sql_query_executor.h>

namespace nx::clusterdb::engine {

class Settings;

class Model
{
public:
    Model(const Settings& settings);

    nx::sql::AsyncSqlQueryExecutor& queryExecutor();

private:
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlQueryExecutor;
};

} // namespace nx::clusterdb::engine
