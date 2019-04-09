#include "connector.h"

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>

#include "connection_manager.h"
#include "p2p_sync_settings.h"
#include "transport/command_transport_delegate.h"
#include "transport/transport_manager.h"

namespace nx::clusterdb::engine {

Connector::Connector(
    const std::string& nodeId,
    const SynchronizationSettings& settings,
    transport::TransportManager* transportManager,
    ConnectionManager* connectionManager)
    :
    m_nodeId(nodeId),
    m_settings(settings),
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

void Connector::removeNodeUrl(
    const::std::string& systemId,
    const nx::utils::Url& url)
{
    post([this, systemId, url]()
    {
        auto it = m_nodes.find(url);
        if (it != m_nodes.end() && it->second.systemId == systemId)
        {
            if (it->second.connection)
            {
                it->second.connection->connectionClosedSubscription().removeSubscription(
                    it->second.connectionClosedSubscriptionId);
            }

            m_connectionManager->removeConnection(it->second.connectionId);
            m_nodes.erase(it);
        }
    });
}

void Connector::stopWhileInAioThread()
{
    for (auto& [url, nodeContext]: m_nodes)
    {
        if (nodeContext.connection)
        {
            nodeContext.connection->connectionClosedSubscription().removeSubscription(
                nodeContext.connectionClosedSubscriptionId);
        }
    }

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
    transport::ConnectResultDescriptor result,
    std::unique_ptr<transport::AbstractConnection> connection)
{
    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());
    NodeContext& nodeContext = nodeIter->second;

    auto connector = std::exchange(nodeContext.connector, nullptr);

    if (!connection)
    {
        NX_DEBUG(this, lm("Error connecting to %1. %2").args(url, result));

        nodeContext.retryTimer->start(
            m_settings.nodeConnectRetryTimeout,
            [url, this]() { connectToNodeAsync(url); });
        return;
    }

    registerConnection(url, &nodeContext, std::move(connection));
}

void Connector::registerConnection(
    const nx::utils::Url& url,
    NodeContext* nodeContext,
    std::unique_ptr<transport::AbstractConnection> connection)
{
    using ConnectionWithSequence = transport::CommandTransportDelegateWithData<int>;

    NX_DEBUG(this, "Connection %1 to %2 established successfully",
        nodeContext->connectionId, url);

    connection->connectionClosedSubscription().subscribe(
        [this, url](auto... args) { onConnectionClosed(url, args...); },
        &nodeContext->connectionClosedSubscriptionId);
    nodeContext->connection = connection.get();

    ConnectionManager::ConnectionContext connectionContext;
    connectionContext.originatingNodeId = m_nodeId;
    connectionContext.connectionId = nodeContext->connectionId;
    connectionContext.fullPeerName.systemId = nodeContext->systemId;
    connectionContext.fullPeerName.peerId =
        connection->commonTransportHeaderOfRemoteTransaction().peerId;
    //connectionContext.userAgent = ;
    const auto connectionSequence = ++m_connectionSequence;
    connectionContext.connection =
        std::make_unique<ConnectionWithSequence>(
            std::move(connection),
            connectionSequence);

    m_connectionManager->addNewConnection(std::move(connectionContext));

    // NOTE: Not relying on connectionId.
    m_connectionManager->modifyConnectionByIdSafe(
        nodeContext->connectionId,
        [connectionSequence](auto abstractConnection)
        {
            const auto connection =
                dynamic_cast<ConnectionWithSequence*>(abstractConnection);
            if (!connection || connection->data() != connectionSequence)
                return;

            connection->start();
        });
}

void Connector::onConnectionClosed(
    const nx::utils::Url& url,
    SystemError::ErrorCode systemErrorCode)
{
    NX_ASSERT(isInSelfAioThread());

    NX_DEBUG(this, lm("Connection to %1 has been closed. %2")
        .args(url, SystemError::toString(systemErrorCode)));

    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());

    nodeIter->second.connection = nullptr;
    nodeIter->second.connectionClosedSubscriptionId = nx::utils::kInvalidSubscriptionId;

    nodeIter->second.retryTimer->start(
        m_settings.nodeConnectRetryTimeout,
        [url, this]() { connectToNodeAsync(url); });
}

} // namespace nx::clusterdb::engine
