#include "connector.h"

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include "connection_manager.h"
#include "transport/transport_manager.h"

namespace nx::data_sync_engine {

static constexpr std::chrono::seconds kRetryConnectTimeout(7);

Connector::Connector(
    TransportManager* transportManager,
    ConnectionManager* connectionManager)
    :
    m_transportManager(transportManager),
    m_connectionManager(connectionManager)
{
}

Connector::~Connector()
{
    pleaseStopSync();
}

void Connector::addNodeUrl(
    const std::string& systemId,
    const nx::utils::Url& url)
{
    post(
        [this, systemId, url]()
        {
            NodeContext nodeContext;
            nodeContext.systemId = systemId;
            nodeContext.retryTimer = std::make_unique<nx::network::aio::Timer>();
            m_nodes.emplace(url, std::move(nodeContext));
            connectToNodeAsync(url);
        });
}

void Connector::stopWhileInAioThread()
{
    m_nodes.clear();
}

void Connector::connectToNodeAsync(const nx::utils::Url& url)
{
    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());
    NodeContext& nodeContext = nodeIter->second;

    nodeContext.connectionId = QnUuid::createUuid().toSimpleByteArray().toStdString();

    NX_DEBUG(this, lm("Initiating connection %1 to %2")
        .args(nodeContext.connectionId, url));

    nodeContext.connector = m_transportManager->createConnector(
        nodeContext.systemId,
        nodeContext.connectionId,
        url);
    nodeContext.connector->connect(
        [this, url](auto... result) { onConnectCompletion(url, std::move(result)...); });
}

void Connector::onConnectCompletion(
    const nx::utils::Url& url,
    std::unique_ptr<AbstractTransactionTransport> connection)
{
    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());
    NodeContext& nodeContext = nodeIter->second;

    auto connector = std::exchange(nodeContext.connector, nullptr);

    if (!connection)
    {
        NX_DEBUG(this, lm("Error connecting to %1. %2")
            .args(url, connector->lastErrorText()));

        nodeContext.retryTimer->start(
            kRetryConnectTimeout,
            [url, this]() { connectToNodeAsync(url); });
        return;
    }

    registerConnection(url, nodeContext, std::move(connection));
}

void Connector::registerConnection(
    const nx::utils::Url& url,
    const NodeContext& nodeContext,
    std::unique_ptr<AbstractTransactionTransport> connection)
{
    NX_DEBUG(this, lm("Connection %1 to %2 established successfully")
        .args(nodeContext.connectionId, url));

    nx::utils::SubscriptionId connectionClosedSubscriptionId;
    connection->connectionClosedSubscription().subscribe(
        [this, url](auto... args) { onConnectionClosed(url, args...); },
        &connectionClosedSubscriptionId);

    ConnectionManager::ConnectionContext connectionContext;
    connectionContext.connectionId = nodeContext.connectionId;
    connectionContext.fullPeerName.systemId = nodeContext.systemId;
    connectionContext.fullPeerName.peerId = 
        connection->commonTransportHeaderOfRemoteTransaction().peerId;
    //connectionContext.userAgent = ;
    connectionContext.connection = std::move(connection);

    m_connectionManager->addNewConnection(std::move(connectionContext));
}

void Connector::onConnectionClosed(
    const nx::utils::Url& url,
    SystemError::ErrorCode systemErrorCode)
{
    NX_DEBUG(this, lm("Connection to %1 has been closed. %2")
        .args(url, SystemError::toString(systemErrorCode)));

    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());

    nodeIter->second.retryTimer->start(
        kRetryConnectTimeout,
        [url, this]() { connectToNodeAsync(url); });
}

} // namespace nx::data_sync_engine
