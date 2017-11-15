#include "synchronization_engine.h"

namespace nx {
namespace cdb {
namespace ec2 {

SyncronizationEngine::SyncronizationEngine(
    const QnUuid& moduleGuid,
    const Settings& settings,
    nx::utils::db::AsyncSqlQueryExecutor* const dbManager)
    :
    m_transactionLog(
        moduleGuid,
        dbManager,
        &m_outgoingTransactionDispatcher),
    m_incomingTransactionDispatcher(
        moduleGuid,
        &m_transactionLog),
    m_connectionManager(
        moduleGuid,
        settings,
        &m_transactionLog,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_systemDeletedSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
}

SyncronizationEngine::~SyncronizationEngine()
{
    m_startedAsyncCallsCounter.wait();
}

OutgoingTransactionDispatcher&
    SyncronizationEngine::outgoingTransactionDispatcher()
{
    return m_outgoingTransactionDispatcher;
}

const OutgoingTransactionDispatcher&
    SyncronizationEngine::outgoingTransactionDispatcher() const
{
    return m_outgoingTransactionDispatcher;
}

TransactionLog& SyncronizationEngine::transactionLog()
{
    return m_transactionLog;
}

const TransactionLog& SyncronizationEngine::transactionLog() const
{
    return m_transactionLog;
}

IncomingTransactionDispatcher&
    SyncronizationEngine::incomingTransactionDispatcher()
{
    return m_incomingTransactionDispatcher;
}

const IncomingTransactionDispatcher&
    SyncronizationEngine::incomingTransactionDispatcher() const
{
    return m_incomingTransactionDispatcher;
}

ec2::ConnectionManager& SyncronizationEngine::connectionManager()
{
    return m_connectionManager;
}

const ec2::ConnectionManager& SyncronizationEngine::connectionManager() const
{
    return m_connectionManager;
}

void SyncronizationEngine::subscribeToSystemDeletedNotification(
    nx::utils::Subscription<std::string>& subscription)
{
    using namespace std::placeholders;

    subscription.subscribe(
        std::bind(&SyncronizationEngine::onSystemDeleted, this, _1),
        &m_systemDeletedSubscriptionId);
}

void SyncronizationEngine::unsubscribeFromSystemDeletedNotification(
    nx::utils::Subscription<std::string>& subscription)
{
    subscription.removeSubscription(m_systemDeletedSubscriptionId);
}

void SyncronizationEngine::onSystemDeleted(const std::string& systemId)
{
    // New connections will not be authorized since system is deleted,
    // but existing ones have to be closed.
    m_connectionManager.closeConnectionsToSystem(
        systemId.c_str(),
        [this, systemId, locker = m_startedAsyncCallsCounter.getScopedIncrement()]()
        {
            m_transactionLog.clearTransactionLogCacheForSystem(systemId.c_str());
        });
}

} // namespace ec2
} // namespace cdb
} // namespace nx
