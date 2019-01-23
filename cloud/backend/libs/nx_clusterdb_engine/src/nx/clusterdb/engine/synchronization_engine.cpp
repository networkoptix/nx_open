#include "synchronization_engine.h"

#include <nx/network/url/url_parse_helper.h>

#include "statistics/provider.h"
#include "transport/http_transport_paths.h"

namespace nx::clusterdb::engine {

SyncronizationEngine::SyncronizationEngine(
    const std::string& /*applicationId*/, // TODO: #ak CLOUD-2249.
    const QnUuid& peerId,
    const SynchronizationSettings& settings,
    const ProtocolVersionRange& supportedProtocolRange,
    nx::sql::AsyncSqlQueryExecutor* const dbManager)
    :
    m_peerId(peerId),
    m_outgoingCommandFilter(peerId),
    m_supportedProtocolRange(supportedProtocolRange),
    m_outgoingTransactionDispatcher(m_outgoingCommandFilter),
    m_structureUpdater(dbManager),
    m_commandLog(
        peerId,
        m_supportedProtocolRange,
        dbManager,
        &m_outgoingTransactionDispatcher),
    m_incomingTransactionDispatcher(&m_commandLog),
    m_connectionManager(
        peerId,
        settings,
        m_supportedProtocolRange,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_transportManager(
        m_supportedProtocolRange,
        &m_commandLog,
        m_outgoingCommandFilter,
        peerId.toSimpleByteArray().toStdString()),
    m_connector(
        &m_transportManager,
        &m_connectionManager),
    m_httpTransportAcceptor(
        peerId,
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_webSocketAcceptor(
        peerId,
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_p2pHttpAcceptor(
        peerId.toSimpleString().toStdString(),
        m_supportedProtocolRange,
        &m_commandLog,
        &m_connectionManager,
        m_outgoingCommandFilter),
    m_statisticsProvider(
        m_connectionManager,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_systemDeletedSubscriptionId(nx::utils::kInvalidSubscriptionId),
    m_httpServer(peerId)
{
}

SyncronizationEngine::~SyncronizationEngine()
{
    m_startedAsyncCallsCounter.wait();
}

OutgoingCommandDispatcher&
    SyncronizationEngine::outgoingTransactionDispatcher()
{
    return m_outgoingTransactionDispatcher;
}

const OutgoingCommandDispatcher&
    SyncronizationEngine::outgoingTransactionDispatcher() const
{
    return m_outgoingTransactionDispatcher;
}

CommandLog& SyncronizationEngine::transactionLog()
{
    return m_commandLog;
}

const CommandLog& SyncronizationEngine::transactionLog() const
{
    return m_commandLog;
}

IncomingCommandDispatcher&
    SyncronizationEngine::incomingCommandDispatcher()
{
    return m_incomingTransactionDispatcher;
}

const IncomingCommandDispatcher&
    SyncronizationEngine::incomingCommandDispatcher() const
{
    return m_incomingTransactionDispatcher;
}

ConnectionManager& SyncronizationEngine::connectionManager()
{
    return m_connectionManager;
}

const ConnectionManager& SyncronizationEngine::connectionManager() const
{
    return m_connectionManager;
}

Connector& SyncronizationEngine::connector()
{
    return m_connector;
}

const statistics::Provider& SyncronizationEngine::statisticsProvider() const
{
    return m_statisticsProvider;
}

void SyncronizationEngine::setOutgoingCommandFilter(
    const OutgoingCommandFilterConfiguration& configuration)
{
    m_outgoingCommandFilter.configure(configuration);
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

QnUuid SyncronizationEngine::peerId() const
{
    return m_peerId;
}

void SyncronizationEngine::registerHttpApi(
    const std::string& pathPrefix,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    m_httpServer.registerHandlers(pathPrefix, dispatcher);

    m_httpTransportAcceptor.registerHandlers(pathPrefix, dispatcher);
    m_webSocketAcceptor.registerHandlers(pathPrefix, dispatcher);
    m_p2pHttpAcceptor.registerHandlers(pathPrefix, dispatcher);
}

template<typename ManagerType>
void SyncronizationEngine::registerHttpHandler(
    const std::string& handlerPath,
    typename SyncConnectionRequestHandler<ManagerType>::ManagerFuncType managerFuncPtr,
    ManagerType* manager,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    typedef SyncConnectionRequestHandler<ManagerType> RequestHandlerType;

    dispatcher->registerRequestProcessor<RequestHandlerType>(
        handlerPath.c_str(),
        [managerFuncPtr, manager]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(manager, managerFuncPtr);
        });
}

template<typename ManagerType>
void SyncronizationEngine::registerHttpHandler(
    nx::network::http::Method::ValueType method,
    const std::string& handlerPath,
    typename SyncConnectionRequestHandler<ManagerType>::ManagerFuncType managerFuncPtr,
    ManagerType* manager,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    typedef SyncConnectionRequestHandler<ManagerType> RequestHandlerType;

    dispatcher->registerRequestProcessor<RequestHandlerType>(
        handlerPath.c_str(),
        [managerFuncPtr, manager]() -> std::unique_ptr<RequestHandlerType>
        {
            return std::make_unique<RequestHandlerType>(manager, managerFuncPtr);
        },
        method);
}

void SyncronizationEngine::onSystemDeleted(const std::string& systemId)
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
