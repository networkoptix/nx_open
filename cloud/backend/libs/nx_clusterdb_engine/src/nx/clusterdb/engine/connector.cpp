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
    const std::string& clusterId,
    const nx::utils::Url& url)
{
    post(
        [this, clusterId, url]()
        {
            NodeContext nodeContext;
            nodeContext.clusterId = clusterId;
            nodeContext.retryTimer = std::make_unique<nx::network::aio::Timer>();
            nodeContext.url = url;
            m_nodes.emplace(url, std::move(nodeContext));
            connectToNodeAsync(url);
        });
}

void Connector::removeNodeUrl(
    const::std::string& clusterId,
    const nx::utils::Url& url)
{
    post([this, clusterId, url]()
    {
        auto it = m_nodes.find(url);
        if (it != m_nodes.end() && it->second.clusterId == clusterId)
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

OnConnectCompletedSubscription& Connector::onConnectCompletedSubscription()
{
    return m_onConnectCompletedSubscription;
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

    if (!nodeContext.nodeId.empty() &&
        m_connectionManager->isNodeConnected(nodeContext.clusterId, nodeContext.nodeId))
    {
        NX_VERBOSE(this, "Skipping connection to connected node %1.%2 (%3)",
            nodeContext.nodeId, nodeContext.clusterId, url);
        scheduleConnectRetry(&nodeContext);
        return;
    }

    nodeContext.connectionId = QnUuid::createUuid().toSimpleByteArray().toStdString();

    NX_DEBUG(this, "Initiating connection %1 to %2", nodeContext.connectionId, url);

    nodeContext.connector = m_transportManager->createConnector(
        nodeContext.clusterId,
        nodeContext.connectionId,
        url);
    nodeContext.connector->connect(
        [this, url](auto... result) { onConnectCompletion(url, std::move(result)...); });
}

void Connector::scheduleConnectRetry(NodeContext* nodeContext)
{
    nodeContext->retryTimer->start(
        m_settings.nodeConnectRetryTimeout,
        [url = nodeContext->url, this]() { connectToNodeAsync(url); });
}

void Connector::onConnectCompletion(
    const nx::utils::Url& url,
    transport::ConnectResult result,
    std::unique_ptr<transport::AbstractConnection> connection)
{
    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());
    NodeContext& nodeContext = nodeIter->second;

    m_onConnectCompletedSubscription.notify(url, result);

    auto connector = std::exchange(nodeContext.connector, nullptr);

    if (!connection)
    {
        NX_DEBUG(this, "Error connecting to %1. %2", url, result);
        scheduleConnectRetry(&nodeContext);
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

    nodeContext->nodeId = connection->commonTransportHeaderOfRemoteTransaction().peerId;

    NX_DEBUG(this, "Connection %1 to %2.%3 (%4) established successfully",
        nodeContext->connectionId, nodeContext->nodeId, nodeContext->clusterId, url);

    connection->connectionClosedSubscription().subscribe(
        [this, url](auto&&... args)
        {
            onConnectionClosed(url, std::forward<decltype(args)>(args)...);
        },
        &nodeContext->connectionClosedSubscriptionId);
    auto connectionPtr = connection.get();

    ConnectionManager::ConnectionContext connectionContext;
    connectionContext.originatingNodeId = m_nodeId;
    connectionContext.connectionId = nodeContext->connectionId;
    connectionContext.fullPeerName.clusterId = nodeContext->clusterId;
    connectionContext.fullPeerName.peerId =
        connection->commonTransportHeaderOfRemoteTransaction().peerId;
    // connectionContext.userAgent = connection->;
    const auto connectionSequence = ++m_connectionSequence;
    connectionContext.connection =
        std::make_unique<ConnectionWithSequence>(
            std::move(connection),
            connectionSequence);

    if (m_connectionManager->addNewConnection(std::move(connectionContext)))
    {
        nodeContext->connection = connectionPtr;
        // NOTE: Starting connection only after it has been added to the ConnectionManager.
        // Not relying on connectionId.
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
    else
    {
        NX_DEBUG(this, "Failed to register connection %1 to %2", nodeContext->connectionId, url);
        nodeContext->connection = nullptr;
        onConnectionClosed(url, SystemError::interrupted);
    }
}

void Connector::onConnectionClosed(
    const nx::utils::Url& url,
    SystemError::ErrorCode systemErrorCode)
{
    NX_ASSERT(isInSelfAioThread());

    NX_DEBUG(this, "Connection to %1 has been closed. %2",
        url, SystemError::toString(systemErrorCode));

    auto nodeIter = m_nodes.find(url);
    NX_CRITICAL(nodeIter != m_nodes.end());

    nodeIter->second.connection = nullptr;
    nodeIter->second.connectionClosedSubscriptionId = nx::utils::kInvalidSubscriptionId;

    nodeIter->second.retryTimer->cancelSync();
    scheduleConnectRetry(&nodeIter->second);
}

} // namespace nx::clusterdb::engine
