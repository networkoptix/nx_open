#pragma once

#include <memory>

#include <nx/sql/async_sql_query_executor.h>

namespace nx::data_sync_engine {

class Settings;

class Model
{
public:
    Model(const Settings& settings);

    nx::sql::AsyncSqlQueryExecutor& queryExecutor();

private:
    std::unique_ptr<nx::sql::AsyncSqlQueryExecutor> m_sqlQueryExecutor;
};

} // namespace nx::data_sync_engine
