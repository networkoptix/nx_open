#pragma once

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::clusterdb::map {

class EventProvider
{
public:
    EventProvider(nx::sql::AsyncSqlQueryExecutor* dbManager);

private:
    //nx::sql::AsyncSqlQueryExecutor* m_dbManager;
};

} // namespace nx::clusterdb::map
