#include "customer_manager.h"

#include "dao/customer.h"

namespace nx::data_sync_engine::test {

CustomerManager::CustomerManager(
    SyncronizationEngine* syncronizationEngine,
    dao::CustomerDao* customerDao)
    :
    m_syncronizationEngine(syncronizationEngine),
    m_customerDao(customerDao)
{
    using namespace std::placeholders;

    m_syncronizationEngine->incomingTransactionDispatcher()
        .registerTransactionHandler<command::SaveCustomer>(
            [this](auto&&... args) { return processSaveCustomer(std::move(args)...); });

    m_syncronizationEngine->incomingTransactionDispatcher()
        .registerTransactionHandler<command::RemoveCustomer>(
            [this](auto&&... args) { return processRemoveCustomer(std::move(args)...); });
}

CustomerManager::~CustomerManager()
{
    m_syncronizationEngine->incomingTransactionDispatcher()
        .removeHandler<command::RemoveCustomer>();

    m_syncronizationEngine->incomingTransactionDispatcher()
        .removeHandler<command::SaveCustomer>();
}

void CustomerManager::saveCustomer(
    const Customer& customer,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().startDbTransaction(
        "",
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

void CustomerManager::removeCustomer(
    const std::string& id,
    nx::utils::MoveOnlyFunc<void(ResultCode)> handler)
{
    using namespace std::placeholders;

    m_syncronizationEngine->transactionLog().startDbTransaction(
        "",
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
            queryContext, "", customer);
}

nx::sql::DBResult CustomerManager::removeCustomerFromDb(
    nx::sql::QueryContext* queryContext,
    const std::string& customerId)
{
    m_customerDao->removeCustomer(queryContext, customerId);

    return m_syncronizationEngine->transactionLog()
        .generateTransactionAndSaveToLog<command::RemoveCustomer>(
            queryContext, "", Id{customerId});
}

nx::sql::DBResult CustomerManager::processSaveCustomer(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    data_sync_engine::Command<Customer> command)
{
    m_customerDao->saveCustomer(queryContext, command.params);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult CustomerManager::processRemoveCustomer(
    nx::sql::QueryContext* queryContext,
    const std::string& /*systemId*/,
    data_sync_engine::Command<Id> command)
{
    m_customerDao->removeCustomer(queryContext, command.params.id);
    return nx::sql::DBResult::ok;
}

} // namespace nx::data_sync_engine::test
