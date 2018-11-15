#pragma once

#include <nx/sql/async_sql_query_executor.h>

#include "../data.h"

namespace nx::clusterdb::engine::test::dao {

class CustomerDao
{
public:
    CustomerDao(nx::sql::AsyncSqlQueryExecutor* queryExecutor);

    void saveCustomer(
        nx::sql::QueryContext* queryContext,
        const Customer& customer);

    void removeCustomer(
        nx::sql::QueryContext* queryContext,
        const std::string& customerId);

    Customers fetchCustomers(nx::sql::QueryContext* queryContext);

    nx::sql::AsyncSqlQueryExecutor& queryExecutor();

private:
    nx::sql::AsyncSqlQueryExecutor* m_queryExecutor;
};

} // namespace nx::clusterdb::engine::test::dao
