#pragma once

#include <set>
#include <map>
#include <string>
#include <optional>

namespace nx::sql { class QueryContext; }

namespace nx::clusterdb::map::dao {

class NX_KEY_VALUE_DB_API KeyValueDao
{
public:
    void insertOrUpdate(
        nx::sql::QueryContext* queryContext,
        const std::string& key,
        const std::string& value);

    void remove(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    std::optional<std::string> get(
        nx::sql::QueryContext* queryContext,
        const std::string& key);

    std::map<std::string, std::string> getPairs(
        nx::sql::QueryContext* queryContext);
};

} // namespace nx::clusterdb::map::dao
