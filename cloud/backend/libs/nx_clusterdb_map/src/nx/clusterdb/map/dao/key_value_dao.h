#pragma once

#include <string>
#include <optional>

namespace nx::sql {

class QueryContext;
class AsyncSqlQueryExecutor;

}

namespace nx::clusterdb::map::dao {

class NX_KEY_VALUE_DB_API KeyValueDao
{
public:
    KeyValueDao(nx::sql::AsyncSqlQueryExecutor* queryExecutor);

    void save(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    void remove(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    std::optional<std::string> get(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    nx::sql::AsyncSqlQueryExecutor& queryExecutor();

private:
    nx::sql::AsyncSqlQueryExecutor* m_queryExecutor;
};

} // namespace nx::clusterdb::map::dao
