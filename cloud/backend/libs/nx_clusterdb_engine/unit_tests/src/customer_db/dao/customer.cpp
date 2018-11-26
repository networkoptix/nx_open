#include "customer.h"

#include <nx/fusion/serialization/sql.h>
#include <nx/sql/query.h>

namespace nx::clusterdb::engine::test::dao {

CustomerDao::CustomerDao(
    nx::sql::AsyncSqlQueryExecutor* queryExecutor)
    :
    m_queryExecutor(queryExecutor)
{
}

void CustomerDao::saveCustomer(
    nx::sql::QueryContext* queryContext,
    const Customer& customer)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        REPLACE INTO customer(id, full_name, address)
        VALUES(:id, :fullName, :address)
    )sql");

    query->bindValue(":id", customer.id);
    query->bindValue(":fullName", customer.fullName);
    query->bindValue(":address", customer.address);

    query->exec();
}

void CustomerDao::removeCustomer(
    nx::sql::QueryContext* queryContext,
    const std::string& customerId)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        DELETE FROM customer WHERE id=:id
    )sql");

    query->bindValue(":id", customerId);

    query->exec();
}

Customers CustomerDao::fetchCustomers(
    nx::sql::QueryContext* queryContext)
{
    auto query = queryContext->connection()->createQuery();
    query->prepare(R"sql(
        SELECT id, full_name, address 
        FROM customer
        ORDER BY id
    )sql");
    query->exec();

    Customers customers;
    while (query->next())
    {
        Customer customer;
        customer.id = query->value("id").toString().toStdString();
        customer.fullName = query->value("full_name").toString().toStdString();
        customer.address = query->value("address").toString().toStdString();
        customers.push_back(std::move(customer));
    }

    return customers;
}

nx::sql::AsyncSqlQueryExecutor& CustomerDao::queryExecutor()
{
    return *m_queryExecutor;
}

} // namespace nx::clusterdb::engine::test::dao
