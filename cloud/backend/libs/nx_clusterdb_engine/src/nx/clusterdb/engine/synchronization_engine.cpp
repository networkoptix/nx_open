#include "synchronization_engine.h"

#include "p2p_sync_settings.h"
#include "statistics/provider.h"

namespace nx::clusterdb::engine {

SynchronizationEngine::SynchronizationEngine(
    const std::string& /*applicationId*/, // TODO: #ak CLOUD-2249.
    const SynchronizationSettings& settings,
    const ProtocolVersionRange& supportedProtocolRange,
    nx::sql::AsyncSqlQueryExecutor* const dbManager)
    :
    m_peerId(
        !QnUuid::fromStringSafe(settings.nodeId).isNull()
            ? QnUuid::fromStringSafe(settings.nodeId)
            : QnUuid::fromArbitraryData(settings.nodeId)),
    m_outgoingCommandFilter(m_peerId),
    m_supportedProtocolRange(supportedProtocolRange),
    m_outgoingTransactionDispatcher(m_outgoingCommandFilter),
    m_structureUpdater(dbManager),
    m_commandLog(
        m_peerId,
        m_supportedProtocolRange,
        dbManager,
        &m_outgoingTransactionDispatcher),
    m_incomingTransactionDispatcher(&m_commandLog),
    m_connectionManager(
        m_peerId,
        settings,
        m_supportedProtocolRange,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_transportManager(
        m_supportedProtocolRange,
        &m_commandLog,
        m_outgoingCommandFilter,
        m_peerId.toSimpleByteArray().toStdString()),
    m_connector(
        &m_transportManager,
        &m_connectionManager),
    m_httpTransportAcceptor(
        m_peerId,
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_webSocketAcceptor(
        m_peerId,
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_p2pHttpAcceptor(
        m_peerId.toSimpleString().toStdString(),
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_statisticsProvider(
        m_connectionManager,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_systemDeletedSubscriptionId(nx::utils::kInvalidSubscriptionId),
    m_httpServer(m_peerId),
    m_discoveryManager(settings.discovery, this)
{
}

SynchronizationEngine::~SynchronizationEngine()
{
    m_startedAsyncCallsCounter.wait();
}

OutgoingCommandDispatcher&
    SynchronizationEngine::outgoingTransactionDispatcher()
{
    return m_outgoingTransactionDispatcher;
}

const OutgoingCommandDispatcher&
    SynchronizationEngine::outgoingTransactionDispatcher() const
{
    return m_outgoingTransactionDispatcher;
}

CommandLog& SynchronizationEngine::transactionLog()
{
    return m_commandLog;
}

const CommandLog& SynchronizationEngine::transactionLog() const
{
    return m_commandLog;
}

IncomingCommandDispatcher&
    SynchronizationEngine::incomingCommandDispatcher()
{
    return m_incomingTransactionDispatcher;
}

const IncomingCommandDispatcher&
    SynchronizationEngine::incomingCommandDispatcher() const
{
    return m_incomingTransactionDispatcher;
}

ConnectionManager& SynchronizationEngine::connectionManager()
{
    return m_connectionManager;
}

const ConnectionManager& SynchronizationEngine::connectionManager() const
{
    return m_connectionManager;
}

Connector& SynchronizationEngine::connector()
{
    return m_connector;
}

const statistics::Provider& SynchronizationEngine::statisticsProvider() const
{
    return m_statisticsProvider;
}

void SynchronizationEngine::setOutgoingCommandFilter(
    const OutgoingCommandFilterConfiguration& configuration)
{
    m_outgoingCommandFilter.configure(configuration);
}

void SynchronizationEngine::subscribeToSystemDeletedNotification(
    nx::utils::Subscription<std::string>& subscription)
{
    using namespace std::placeholders;

    subscription.subscribe(
        std::bind(&SynchronizationEngine::onSystemDeleted, this, _1),
        &m_systemDeletedSubscriptionId);
}

void SynchronizationEngine::unsubscribeFromSystemDeletedNotification(
    nx::utils::Subscription<std::string>& subscription)
{
    subscription.removeSubscription(m_systemDeletedSubscriptionId);
}

QnUuid SynchronizationEngine::peerId() const
{
    return m_peerId;
}

void SynchronizationEngine::registerHttpApi(
    const std::string& pathPrefix,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    m_httpServer.registerHandlers(pathPrefix, dispatcher);

    m_httpTransportAcceptor.registerHandlers(pathPrefix, dispatcher);
    m_webSocketAcceptor.registerHandlers(pathPrefix, dispatcher);
    m_p2pHttpAcceptor.registerHandlers(pathPrefix, dispatcher);
}

DiscoveryManager& SynchronizationEngine::discoveryManager()
{
    return m_discoveryManager;
}

void SynchronizationEngine::onSystemDeleted(const std::string& systemId)
{
    // New connections will not be authorized since system is deleted,
    // but existing ones have to be closed.
    m_connectionManager.closeConnectionsToSystem(
        systemId.c_str(),
        [this, systemId, locker = m_startedAsyncCallsCounter.getScopedIncrement()]()
        {
            m_commandLog.markSystemForDeletion(systemId.c_str());
        });
}

} // namespace nx::clusterdb::engine
