#include "synchronization_engine.h"

#include <nx/network/url/url_parse_helper.h>

#include "statistics/provider.h"

namespace nx {
namespace data_sync_engine {

SyncronizationEngine::SyncronizationEngine(
    const std::string& /*applicationId*/, // TODO: #ak CLOUD-2249.
    const QnUuid& moduleGuid,
    const SynchronizationSettings& settings,
    const ProtocolVersionRange& supportedProtocolRange,
    nx::sql::AsyncSqlQueryExecutor* const dbManager)
    :
    m_supportedProtocolRange(supportedProtocolRange),
    m_structureUpdater(dbManager),
    m_transactionLog(
        moduleGuid,
        m_supportedProtocolRange,
        dbManager,
        &m_outgoingTransactionDispatcher),
    m_incomingTransactionDispatcher(
        moduleGuid,
        &m_transactionLog),
    m_connectionManager(
        moduleGuid,
        settings,
        m_supportedProtocolRange,
        &m_transactionLog,
        &m_incomingTransactionDispatcher,
        &m_outgoingTransactionDispatcher),
    m_httpTransportAcceptor(
        moduleGuid,
        m_supportedProtocolRange,
        &m_transactionLog,
        &m_connectionManager),
    m_webSocketAcceptor(
        moduleGuid,
        m_supportedProtocolRange,
        &m_transactionLog,
        &m_connectionManager),
    m_statisticsProvider(
        m_connectionManager,
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

ConnectionManager& SyncronizationEngine::connectionManager()
{
    return m_connectionManager;
}

const ConnectionManager& SyncronizationEngine::connectionManager() const
{
    return m_connectionManager;
}

const statistics::Provider& SyncronizationEngine::statisticsProvider() const
{
    return m_statisticsProvider;
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

static constexpr char kEstablishEc2TransactionConnectionPath[] = "/events/ConnectingStage1";
static constexpr char kEstablishEc2P2pTransactionConnectionPath[] = "/messageBus";
static constexpr char kPushEc2TransactionPath[] = "/forward_events/{sequence}";

void SyncronizationEngine::registerHttpApi(
    const std::string& pathPrefix,
    nx::network::http::server::rest::MessageDispatcher* dispatcher)
{
    registerHttpHandler(
        nx::network::url::joinPath(pathPrefix, kEstablishEc2TransactionConnectionPath),
        &transport::HttpTransportAcceptor::createConnection,
        &m_httpTransportAcceptor,
        dispatcher);

    registerHttpHandler(
        nx::network::url::joinPath(pathPrefix, kPushEc2TransactionPath),
        &transport::HttpTransportAcceptor::pushTransaction,
        &m_httpTransportAcceptor,
        dispatcher);

    registerHttpHandler(
        nx::network::http::Method::get,
        nx::network::url::joinPath(pathPrefix, kEstablishEc2P2pTransactionConnectionPath),
        &transport::WebSocketTransportAcceptor::createConnection,
        &m_webSocketAcceptor,
        dispatcher);
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
            m_transactionLog.clearTransactionLogCacheForSystem(systemId.c_str());
        });
}

} // namespace data_sync_engine
} // namespace nx
