#include "connection_manager.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

#include <nx_ec/data/api_fwd.h>

#include "command_descriptor.h"
#include "compatible_ec2_protocol_version.h"
#include "incoming_command_dispatcher.h"
#include "outgoing_command_dispatcher.h"
#include "p2p_sync_settings.h"

namespace nx::clusterdb::engine {

ConnectionManager::ConnectionManager(
    const std::string& nodeId,
    const SynchronizationSettings& settings,
    const ProtocolVersionRange& protocolVersionRange,
    IncomingCommandDispatcher* const transactionDispatcher,
    OutgoingCommandDispatcher* const outgoingTransactionDispatcher)
:
    m_nodeId(nodeId),
    m_settings(settings),
    m_protocolVersionRange(protocolVersionRange),
    m_transactionDispatcher(transactionDispatcher),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_localPeerData(
        QnUuid::fromStringSafe(nodeId),
        QnUuid::createUuid(),
        vms::api::PeerType::cloudServer,
        Qn::UbjsonFormat),
    m_onNewTransactionSubscriptionId(nx::utils::kInvalidSubscriptionId)
{
    m_outgoingTransactionDispatcher->onNewTransactionSubscription().subscribe(
        [this](auto&&... args) { dispatchTransaction(std::move(args)...); },
        &m_onNewTransactionSubscriptionId);
}

ConnectionManager::~ConnectionManager()
{
    m_outgoingTransactionDispatcher->onNewTransactionSubscription()
        .removeSubscription(m_onNewTransactionSubscriptionId);

    pleaseStopSync();
}

void ConnectionManager::pleaseStopSync()
{
    ConnectionDict localConnections;
    {
        QnMutexLocker lk(&m_mutex);
        std::swap(localConnections, m_connections);
    }

    for (auto it = localConnections.begin(); it != localConnections.end(); ++it)
        it->connection->pleaseStopSync();

    m_startedAsyncCallsCounter.wait();
}

void ConnectionManager::dispatchTransaction(
    const std::string& clusterId,
    std::shared_ptr<const SerializableAbstractCommand> transactionSerializer)
{
    NX_VERBOSE(this, "clusterId %1. Dispatching command %2",
        clusterId, transactionSerializer->header());

    // Generating transport header.
    CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.clusterId = clusterId;
    transportHeader.vmsTransportHeader.distance = 1;
    transportHeader.vmsTransportHeader.processedPeers.insert(m_localPeerData.id);
    // transportHeader.vmsTransportHeader.dstPeers.insert(); TODO:.

    QnMutexLocker lk(&m_mutex);
    const auto& connectionByClusterIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    std::size_t connectionCount = 0;
    std::array<transport::AbstractConnection*, 7> connectionsToSendTo;
    for (auto connectionIt = connectionByClusterIdAndPeerIdIndex
            .lower_bound(FullPeerName{clusterId, std::string()});
        connectionIt != connectionByClusterIdAndPeerIdIndex.end()
            && connectionIt->fullPeerName.clusterId == clusterId;
        ++connectionIt)
    {
        if (connectionIt->fullPeerName.peerId ==
                transactionSerializer->header().peerID.toStdString())
        {
            // Not sending transaction to peer which has generated it.
            continue;
        }

        connectionsToSendTo[connectionCount++] = connectionIt->connection.get();
        if (connectionCount < connectionsToSendTo.size())
            continue;

        for (auto& connection: connectionsToSendTo)
        {
            connection->sendTransaction(
                transportHeader,
                transactionSerializer);
        }
        connectionCount = 0;
    }

    if (connectionCount == 1)
    {
        // Minor optimization.
        connectionsToSendTo[0]->sendTransaction(
            std::move(transportHeader),
            std::move(transactionSerializer));
    }
    else
    {
        for (std::size_t i = 0; i < connectionCount; ++i)
        {
            connectionsToSendTo[i]->sendTransaction(
                transportHeader,
                transactionSerializer);
        }
    }
}

std::vector<ConnectionInfo> ConnectionManager::getConnections() const
{
    QnMutexLocker lk(&m_mutex);

    std::vector<ConnectionInfo> result;
    result.reserve(m_connections.size());
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        result.push_back({
            it->fullPeerName.clusterId,
            it->fullPeerName.peerId,
            it->connection->remotePeerEndpoint(),
            it->userAgent});
    }

    return result;
}

std::size_t ConnectionManager::getConnectionCount() const
{
    QnMutexLocker lk(&m_mutex);
    return m_connections.size();
}

bool ConnectionManager::isClusterConnected(const std::string& clusterId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionByClusterIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    const auto clusterIter = connectionByClusterIdAndPeerIdIndex.lower_bound(
        FullPeerName{clusterId, std::string()});

    return
        clusterIter != connectionByClusterIdAndPeerIdIndex.end() &&
        clusterIter->fullPeerName.clusterId == clusterId;
}

bool ConnectionManager::isNodeConnected(
    const std::string& clusterId,
    const std::string& nodeId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionByClusterIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    const auto clusterIter = connectionByClusterIdAndPeerIdIndex.find(
        FullPeerName{clusterId, nodeId});

    return clusterIter != connectionByClusterIdAndPeerIdIndex.end();
}

unsigned int ConnectionManager::getConnectionCountByClusterId(
    const std::string& clusterId) const
{
    QnMutexLocker lock(&m_mutex);
    return getConnectionCountByClusterId(lock, clusterId);
}

void ConnectionManager::closeConnectionsToCluster(
    const std::string& clusterId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    NX_DEBUG(this, "Closing all connections to cluster %1", clusterId);

    auto allConnectionsRemovedGuard =
        nx::utils::makeSharedGuard(std::move(completionHandler));

    QnMutexLocker lock(&m_mutex);

    auto& connectionByClusterIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    auto it = connectionByClusterIdAndPeerIdIndex.lower_bound(
        FullPeerName{clusterId, std::string()});
    while (it != connectionByClusterIdAndPeerIdIndex.end() &&
           it->fullPeerName.clusterId == clusterId)
    {
        removeConnectionByIter(
            lock,
            connectionByClusterIdAndPeerIdIndex,
            it++,
            [allConnectionsRemovedGuard](){});
    }
}

ConnectionManager::ClusterStatusChangedSubscription&
    ConnectionManager::clusterStatusChangedSubscription()
{
    return m_clusterStatusChangedSubscription;
}

ConnectionManager::NodeStatusChangedSubscription&
    ConnectionManager::nodeStatusChangedSubscription()
{
    return m_nodeStatusChangedSubscription;
}

bool ConnectionManager::addNewConnection(ConnectionContext context)
{
    QnMutexLocker lock(&m_mutex);

    const auto clusterWasOffline = getConnectionCountByClusterId(
        lock, context.fullPeerName.clusterId) == 0;

    if (!authorizeNewConnection(lock, context))
        return false;

    nx::utils::SubscriptionId subscriptionId;
    context.connection->connectionClosedSubscription().subscribe(
        [this, id = context.connectionId](auto&&... /*args*/){ removeConnection(id); },
        &subscriptionId);
    context.connection->setOnGotTransaction(
        [this, id = context.connectionId](auto&&... args)
        {
            onGotTransaction(id, std::move(args)...);
        });

    NX_DEBUG(this, lm("Adding new transaction connection %1 from %2")
        .arg(context.connectionId)
        .arg(context.connection->commonTransportHeaderOfRemoteTransaction()));

    const auto clusterId = context.fullPeerName.clusterId;
    const auto protocolVersion = context.connection->
        commonTransportHeaderOfRemoteTransaction().transactionFormatVersion;

    const auto fullPeerName = context.fullPeerName;

    if (!m_connections.insert(std::move(context)).second)
    {
        NX_ASSERT(false);
        return false;
    }

    lock.unlock();

    m_nodeStatusChangedSubscription.notify(
        {fullPeerName},
        {true /*online*/, protocolVersion});

    if (clusterWasOffline)
    {
        m_clusterStatusChangedSubscription.notify(
            clusterId,
            {true /*online*/, protocolVersion});
    }

    return true;
}

void ConnectionManager::removeConnection(const std::string& connectionId)
{
    NX_VERBOSE(this,
        lm("Removing connection %1").args(connectionId));

    QnMutexLocker lock(&m_mutex);
    removeExistingConnection<kConnectionByIdIndex>(lock, connectionId);
}

bool ConnectionManager::modifyConnectionByIdSafe(
    const std::string& connectionId,
    std::function<void(transport::AbstractConnection* connection)> func)
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionByIdIndex = m_connections.get<kConnectionByIdIndex>();
    auto connectionIter = connectionByIdIndex.find(connectionId);
    if (connectionIter == connectionByIdIndex.end())
        return false; //< Connection could be removed while accepting another connection.

    func(connectionIter->connection.get());
    return true;
}

bool ConnectionManager::authorizeNewConnection(
    const QnMutexLockerBase& lock,
    const ConnectionContext& context)
{
    auto& connectionIndex = m_connections.get<kConnectionByFullPeerNameIndex>();
    const auto existingConnectionIter = connectionIndex.find(context.fullPeerName);
    if (existingConnectionIter == connectionIndex.end())
        return isOneMoreConnectionFromClusterAllowed(lock, context);

    // An existing connection is being overridden by the same node.
    if (existingConnectionIter->originatingNodeId == context.originatingNodeId ||
        // New connection originating node id is greater than that of the existing connection.
        context.originatingNodeId > existingConnectionIter->originatingNodeId)
    {
        NX_DEBUG(this, "Preferring new connection to %1 from node %2 over existing connection from %3",
            context.fullPeerName, context.originatingNodeId,
            existingConnectionIter->originatingNodeId);

        removeConnectionByIter(
            lock,
            connectionIndex,
            existingConnectionIter,
            []() {});
        return true;
    }

    return false;
}

bool ConnectionManager::isOneMoreConnectionFromClusterAllowed(
    const QnMutexLockerBase& lk,
    const ConnectionContext& context) const
{
    const auto existingConnectionCount =
        getConnectionCountByClusterId(lk, context.fullPeerName.clusterId);

    if (existingConnectionCount >= m_settings.maxConcurrentConnectionsFromCluster)
    {
        NX_VERBOSE(this, lm("Refusing connection %1 from %2 since "
                "there are already %3 connections from that cluster")
            .arg(context.connectionId)
            .arg(context.connection->commonTransportHeaderOfRemoteTransaction())
            .arg(existingConnectionCount));
        return false;
    }

    return true;
}

unsigned int ConnectionManager::getConnectionCountByClusterId(
    const QnMutexLockerBase& /*lk*/,
    const std::string& clusterId) const
{
    const auto& connectionByFullPeerName =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    auto it = connectionByFullPeerName.lower_bound(
        FullPeerName{clusterId, std::string()});
    unsigned int activeConnections = 0;
    for (; it != connectionByFullPeerName.end(); ++it)
    {
         if (it->fullPeerName.clusterId != clusterId)
             break;
         ++activeConnections;
    }

    return activeConnections;
}

template<int connectionIndexNumber, typename ConnectionKeyType>
void ConnectionManager::removeExistingConnection(
    const QnMutexLockerBase& lock,
    ConnectionKeyType connectionKey)
{
    auto& connectionIndex = m_connections.get<connectionIndexNumber>();
    auto existingConnectionIter = connectionIndex.find(connectionKey);
    if (existingConnectionIter == connectionIndex.end())
        return;

    removeConnectionByIter(
        lock,
        connectionIndex,
        existingConnectionIter,
        [](){});
}

template<typename ConnectionIndex, typename Iterator, typename CompletionHandler>
void ConnectionManager::removeConnectionByIter(
    const QnMutexLockerBase& /*lock*/,
    ConnectionIndex& connectionIndex,
    Iterator connectionIterator,
    CompletionHandler completionHandler)
{
    std::unique_ptr<transport::AbstractConnection> existingConnection;
    connectionIndex.modify(
        connectionIterator,
        [&existingConnection](ConnectionContext& data)
        {
            existingConnection.swap(data.connection);
        });
    connectionIndex.erase(connectionIterator);

    transport::AbstractConnection* existingConnectionPtr = existingConnection.get();

    NX_DEBUG(this, lm("Removing transaction connection %1 from %2")
            .arg(existingConnectionPtr->connectionGuid())
            .arg(existingConnectionPtr->commonTransportHeaderOfRemoteTransaction()));

    // ::ec2::TransactionTransportBase does not support its removal
    //  in signal handler, so have to remove it delayed.
    // TODO: #ak have to ensure somehow that no events from
    //  connectionContext.connection are delivered.

    existingConnectionPtr->post(
        [this, existingConnection = std::move(existingConnection),
            locker = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]() mutable
        {
            sendClusterOfflineNotificationIfNeeded(
                existingConnection->commonTransportHeaderOfRemoteTransaction());
            existingConnection.reset();
            completionHandler();
        });
}

void ConnectionManager::sendClusterOfflineNotificationIfNeeded(
    const CommandTransportHeader& transportHeader)
{
    m_nodeStatusChangedSubscription.notify(
        {FullPeerName{transportHeader.clusterId, transportHeader.peerId}},
        {false /*offline*/, 0});

    if (getConnectionCountByClusterId(transportHeader.clusterId) == 0)
    {
        m_clusterStatusChangedSubscription.notify(
            transportHeader.clusterId,
            {false /*offline*/, 0});
    }
}

void ConnectionManager::onGotTransaction(
    const std::string& connectionId,
    std::unique_ptr<DeserializableCommandData> commandData,
    CommandTransportHeader transportHeader)
{
    m_transactionDispatcher->dispatchTransaction(
        std::move(transportHeader),
        std::move(commandData),
        [this, locker = m_startedAsyncCallsCounter.getScopedIncrement(), connectionId](
            ResultCode resultCode)
        {
            onTransactionDone(connectionId, resultCode);
        });
}

void ConnectionManager::onTransactionDone(
    const std::string& connectionId,
    ResultCode resultCode)
{
    if (resultCode != ResultCode::ok)
    {
        NX_DEBUG(this, "Closing connection %1 due to failed transaction (result code %2)",
            connectionId, toString(resultCode));

        // Closing connection in case of failure.
        QnMutexLocker lock(&m_mutex);
        removeExistingConnection<kConnectionByIdIndex, std::string>(lock, connectionId);
    }
}

} // namespace nx::clusterdb::engine
