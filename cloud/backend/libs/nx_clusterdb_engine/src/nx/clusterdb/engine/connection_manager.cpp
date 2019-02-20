#include "connection_manager.h"

#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/http/custom_headers.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

#include <nx_ec/data/api_fwd.h>

#include "command_descriptor.h"
#include "compatible_ec2_protocol_version.h"
#include "incoming_transaction_dispatcher.h"
#include "outgoing_transaction_dispatcher.h"
#include "p2p_sync_settings.h"
#include "transaction_transport.h"
#include "transaction_transport_header.h"

namespace nx::clusterdb::engine {

ConnectionManager::ConnectionManager(
    const QnUuid& moduleGuid,
    const SynchronizationSettings& settings,
    const ProtocolVersionRange& protocolVersionRange,
    IncomingCommandDispatcher* const transactionDispatcher,
    OutgoingCommandDispatcher* const outgoingTransactionDispatcher)
:
    m_settings(settings),
    m_protocolVersionRange(protocolVersionRange),
    m_transactionDispatcher(transactionDispatcher),
    m_outgoingTransactionDispatcher(outgoingTransactionDispatcher),
    m_localPeerData(
        moduleGuid,
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
    const std::string& systemId,
    std::shared_ptr<const SerializableAbstractCommand> transactionSerializer)
{
    NX_VERBOSE(this, lm("systemId %1. Dispatching command %2")
        .args(systemId, toString(transactionSerializer->header())));

    // Generating transport header.
    CommandTransportHeader transportHeader(m_protocolVersionRange.currentVersion());
    transportHeader.systemId = systemId;
    transportHeader.vmsTransportHeader.distance = 1;
    transportHeader.vmsTransportHeader.processedPeers.insert(m_localPeerData.id);
    // transportHeader.vmsTransportHeader.dstPeers.insert(); TODO:.

    QnMutexLocker lk(&m_mutex);
    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    std::size_t connectionCount = 0;
    std::array<transport::AbstractConnection*, 7> connectionsToSendTo;
    for (auto connectionIt = connectionBySystemIdAndPeerIdIndex
            .lower_bound(FullPeerName{systemId, std::string()});
        connectionIt != connectionBySystemIdAndPeerIdIndex.end()
            && connectionIt->fullPeerName.systemId == systemId;
        ++connectionIt)
    {
        if (connectionIt->fullPeerName.peerId ==
                transactionSerializer->header().peerID.toByteArray())
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

std::vector<SystemConnectionInfo> ConnectionManager::getConnections() const
{
    QnMutexLocker lk(&m_mutex);

    std::vector<SystemConnectionInfo> result;
    result.reserve(m_connections.size());
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        result.push_back({
            it->fullPeerName.systemId,
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

bool ConnectionManager::isSystemConnected(const std::string& systemId) const
{
    QnMutexLocker lk(&m_mutex);

    const auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    const auto systemIter = connectionBySystemIdAndPeerIdIndex.lower_bound(
        FullPeerName{systemId, std::string()});

    return
        systemIter != connectionBySystemIdAndPeerIdIndex.end() &&
        systemIter->fullPeerName.systemId == systemId;
}

unsigned int ConnectionManager::getConnectionCountBySystemId(
    const std::string& systemId) const
{
    QnMutexLocker lock(&m_mutex);
    return getConnectionCountBySystemId(lock, systemId);
}

void ConnectionManager::closeConnectionsToSystem(
    const std::string& systemId,
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    NX_DEBUG(this,
        lm("Closing all connections to system %1").args(systemId));

    auto allConnectionsRemovedGuard =
        nx::utils::makeSharedGuard(std::move(completionHandler));

    QnMutexLocker lock(&m_mutex);

    auto& connectionBySystemIdAndPeerIdIndex =
        m_connections.get<kConnectionByFullPeerNameIndex>();
    auto it = connectionBySystemIdAndPeerIdIndex.lower_bound(
        FullPeerName{systemId, std::string()});
    while (it != connectionBySystemIdAndPeerIdIndex.end() &&
           it->fullPeerName.systemId == systemId)
    {
        removeConnectionByIter(
            lock,
            connectionBySystemIdAndPeerIdIndex,
            it++,
            [allConnectionsRemovedGuard](){});
    }
}

ConnectionManager::SystemStatusChangedSubscription&
    ConnectionManager::systemStatusChangedSubscription()
{
    return m_systemStatusChangedSubscription;
}

bool ConnectionManager::addNewConnection(ConnectionContext context)
{
    QnMutexLocker lock(&m_mutex);

    const auto systemWasOffline = getConnectionCountBySystemId(
        lock, context.fullPeerName.systemId) == 0;

    // TODO: #ak "One connection only" logic MUST be configurable.
    removeExistingConnection<
        kConnectionByFullPeerNameIndex,
        decltype(context.fullPeerName)>(lock, context.fullPeerName);

    if (!isOneMoreConnectionFromSystemAllowed(lock, context))
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

    const auto systemId = context.fullPeerName.systemId;
    const auto protocolVersion = context.connection->
        commonTransportHeaderOfRemoteTransaction().transactionFormatVersion;

    if (!m_connections.insert(std::move(context)).second)
    {
        NX_ASSERT(false);
        return false;
    }

    if (systemWasOffline)
    {
        lock.unlock();
        m_systemStatusChangedSubscription.notify(
            systemId,
            { true /*online*/, protocolVersion });
    }

    return true;
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

bool ConnectionManager::isOneMoreConnectionFromSystemAllowed(
    const QnMutexLockerBase& lk,
    const ConnectionContext& context) const
{
    const auto existingConnectionCount =
        getConnectionCountBySystemId(lk, context.fullPeerName.systemId);

    if (existingConnectionCount >= m_settings.maxConcurrentConnectionsFromSystem)
    {
        NX_VERBOSE(this, lm("Refusing connection %1 from %2 since "
                "there are already %3 connections from that system")
            .arg(context.connectionId)
            .arg(context.connection->commonTransportHeaderOfRemoteTransaction())
            .arg(existingConnectionCount));
        return false;
    }

    return true;
}

unsigned int ConnectionManager::getConnectionCountBySystemId(
    const QnMutexLockerBase& /*lk*/,
    const std::string& systemId) const
{
    const auto& connectionByFullPeerName =
        m_connections.get<kConnectionByFullPeerNameIndex>();

    auto it = connectionByFullPeerName.lower_bound(
        FullPeerName{systemId, std::string()});
    unsigned int activeConnections = 0;
    for (; it != connectionByFullPeerName.end(); ++it)
    {
         if (it->fullPeerName.systemId != systemId)
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
            sendSystemOfflineNotificationIfNeeded(
                existingConnection->commonTransportHeaderOfRemoteTransaction().systemId);
            existingConnection.reset();
            completionHandler();
        });
}

void ConnectionManager::sendSystemOfflineNotificationIfNeeded(
    const std::string& systemId)
{
    if (getConnectionCountBySystemId(systemId) > 0)
        return;

    m_systemStatusChangedSubscription.notify(
        systemId, { false /*offline*/, 0 });
}

void ConnectionManager::removeConnection(const std::string& connectionId)
{
    NX_VERBOSE(this,
        lm("Removing connection %1").args(connectionId));

    QnMutexLocker lock(&m_mutex);
    removeExistingConnection<kConnectionByIdIndex>(lock, connectionId);
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
        NX_DEBUG(this,
            lm("Closing connection %1 due to failed transaction (result code %2)")
                .args(connectionId, toString(resultCode)));

        // Closing connection in case of failure.
        QnMutexLocker lock(&m_mutex);
        removeExistingConnection<kConnectionByIdIndex, std::string>(lock, connectionId);
    }
}

} // namespace nx::clusterdb::engine
