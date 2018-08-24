#pragma once

namespace nx::sql { class AsyncSqlQueryExecutor; }

namespace nx::kvdb {

class EventProvider
{
public:
    EventProvider(nx::sql::AsyncSqlQueryExecutor* dbManager);

private:
    nx::sql::AsyncSqlQueryExecutor* m_dbManager;
};

} // namespace nx::kvdb
