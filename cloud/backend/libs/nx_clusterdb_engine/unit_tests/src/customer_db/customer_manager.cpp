#include "customer_manager.h"

#include "dao/customer.h"

namespace nx::clusterdb::engine::test {

std::string toString(ResultCode result)
{
    switch (result)
    {
        case ResultCode::ok:
            return "ok";
        case ResultCode::error:
            return "error";
    }

    return "unknown";
}

//-------------------------------------------------------------------------------------------------

CustomerManager::CustomerManager(
    SyncronizationEngine* syncronizationEngine,
    dao::CustomerDao* customerDao,
    const std::string& systemId)
    :
    m_syncronizationEngine(syncronizationEngine),
    m_customerDao(customerDao),
    m_systemId(systemId)
{
    using namespace std::placeholders;

    m_syncronizationEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::SaveCustomer>(
            [this](auto&&... args) { return processSaveCustomer(std::move(args)...); });

    m_syncronizationEngine->incomingCommandDispatcher()
        .registerCommandHandler<command::RemoveCustomer>(
            [this](auto&&... args) { return processRemoveCustomer(std::move(args)...); });
}

CustomerManager::~CustomerManager()
{
    m_syncronizationEngine->incomingCommandDispatcher()
        .removeHandler<command::RemoveCustomer>();

    m_syncronizationEngine->incomingCommandDispatcher()
        .removeHandler<command::SaveCustomer>();
}

void CustomerManager::saveCustomer(
    const Customer& customer,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().startDbTransaction(
        m_systemId,
        [this, customer](auto&&... args)
        {
            return saveCustomerToDb(std::move(args)..., customer);
        },
        [handler = std::move(handler)](nx::sql::DBResult dbResult)
        {
            handler(dbResult == nx::sql::DBResult::ok
                ? ResultCode::ok
                : ResultCode::error);
        });
}

ResultCode CustomerManager::saveCustomer(const Customer& customer)
{
    std::promise<ResultCode> done;
    saveCustomer(
        customer,
        [&done](ResultCode resultCode) { done.set_value(resultCode); });
    return done.get_future().get();
}

void CustomerManager::removeCustomer(
    const std::string& id,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().startDbTransaction(
        m_systemId,
        [this, id](auto&&... args) { return removeCustomerFromDb(std::move(args)..., id); },
        [handler = std::move(handler)](nx::sql::DBResult dbResult)
        {
            handler(dbResult == nx::sql::DBResult::ok
                ? ResultCode::ok
                : ResultCode::error);
        });
}

void CustomerManager::getCustomers(
    nx::utils::MoveOnlyFunc<void(ResultCode, Customers /*customers*/)> handler)
{
    auto customers = std::make_shared<Customers>();

    m_customerDao->queryExecutor().executeSelect(
        [this, customers](nx::sql::QueryContext* queryContext)
        {
            *customers = m_customerDao->fetchCustomers(queryContext);
            return nx::sql::DBResult::ok;
        },
        [this, customers, handler = std::move(handler)](
            nx::sql::DBResult dbResult)
        {
            handler(
                dbResult == nx::sql::DBResult::ok ? ResultCode::ok : ResultCode::error,
                std::move(*customers));
        });
}

nx::sql::DBResult CustomerManager::saveCustomerToDb(
    nx::sql::QueryContext* queryContext,
    const Customer& customer)
{
    if (customer.id.empty())
        return nx::sql::DBResult::logicError;

    m_customerDao->saveCustomer(queryContext, customer);

    return m_syncronizationEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::SaveCustomer>(
            queryContext, m_systemId, customer);
}

nx::sql::DBResult CustomerManager::removeCustomerFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& customerId)
{
    m_customerDao->removeCustomer(queryContext, customerId);

    return m_syncronizationEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveCustomer>(
            queryContext, m_systemId, Id{customerId});
}

nx::sql::DBResult CustomerManager::processSaveCustomer(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<Customer> command)
{
    m_customerDao->saveCustomer(queryContext, command.params);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult CustomerManager::processRemoveCustomer(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    clusterdb::engine::Command<Id> command)
{
    m_customerDao->removeCustomer(queryContext, command.params.id);
    return nx::sql::DBResult::ok;
}

} // namespace nx::clusterdb::engine::test
