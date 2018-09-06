#pragma once

#include <nx/data_sync_engine/synchronization_engine.h>
#include <nx/sql/query_context.h>
#include <nx/utils/move_only_func.h>

#include "data.h"

namespace nx::data_sync_engine::test {

namespace dao { class CustomerDao; }

enum class ResultCode
{
    ok,
    error,
};

class CustomerManager
{
public:
    CustomerManager(
        SyncronizationEngine* syncronizationEngine,
        dao::CustomerDao* customerDao);
    ~CustomerManager();

    void saveCustomer(
        const Customer& customer,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);

    void removeCustomer(
        const std::string& id,
        nx::utils::MoveOnlyFunc<void(ResultCode)> handler);

    void getCustomers(
        nx::utils::MoveOnlyFunc<void(ResultCode, Customers /*customers*/)> handler);

private:
    SyncronizationEngine* m_syncronizationEngine = nullptr;
    dao::CustomerDao* m_customerDao = nullptr;

    nx::sql::DBResult saveCustomerToDb(
        nx::sql::QueryContext* queryContext,
        const Customer& customer);

    nx::sql::DBResult removeCustomerFromDb(
        nx::sql::QueryContext* queryContext,
        const std::string& customerId);

    nx::sql::DBResult processSaveCustomer(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        data_sync_engine::Command<Customer> command);

    nx::sql::DBResult processRemoveCustomer(
        nx::sql::QueryContext* queryContext,
        const nx::String& systemId,
        data_sync_engine::Command<Id> command);
};

} // namespace nx::data_sync_engine::test
