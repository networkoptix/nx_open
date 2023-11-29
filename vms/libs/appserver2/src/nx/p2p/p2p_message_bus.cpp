// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "p2p_message_bus.h"

#include <api/runtime_info_manager.h>
#include <common/static_common_module.h>
#include <nx/cloud/db/api/ec2_request_paths.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/bit_stream.h>
#include <nx/utils/math/math.h>
#include <nx/utils/qmetaobject_helper.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/random.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/std/cmath.h>
#include <nx/vms/api/data/update_sequence_data.h>
#include <nx/vms/api/protocol_version.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/ec2/ec_connection_notification_manager.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/transaction_message_bus_priv.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

namespace nx {
namespace p2p {

const QString MessageBus::kCloudPathPrefix("/cdb");
std::chrono::milliseconds kDefaultTimerInterval(500);

using namespace ec2;
using namespace vms::api;
using namespace nx::utils;

std::chrono::milliseconds MessageBus::DelayIntervals::minInterval() const
{
    const auto data = {
        sendPeersInfoInterval,
        subscribeIntervalLow,
        subscribeIntervalHigh,
        outConnectionsInterval,
        remotePeerReconnectTimeout};
    return *std::min_element(data.begin(), data.end());
}

QString MessageBus::peerName(const QnUuid& id)
{
    return qnStaticCommon->moduleDisplayName(id);
}

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus *bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader,
        nx::Locker<nx::Mutex>* lock) const
    {
        if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
            bus->printTran(connection, transaction, Connection::Direction::incoming);
        bus->gotTransaction(transaction, connection, transportHeader, lock);
    }
};

struct GotUnicastTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus *bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader,
        nx::Locker<nx::Mutex>* lock) const
    {
        bus->gotUnicastTransaction(transaction, connection, transportHeader, lock);
    }
};

// ---------------------- P2pMessageBus --------------

MessageBus::MessageBus(
    nx::vms::common::SystemContext* systemContext,
    vms::api::PeerType peerType,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
    :
    TransactionMessageBusBase(peerType, systemContext, jsonTranSerializer, ubjsonTranSerializer),
    m_miscData(this)
{
    static const int kMetaTypeRegistrator =
        []()
        {
            qRegisterMetaType<MessageType>();
            qRegisterMetaType<ConnectionBase::State>("ConnectionBase::State");
            qRegisterMetaType<P2pConnectionPtr>("P2pConnectionPtr");
            qRegisterMetaType<QWeakPointer<ConnectionBase>>("QWeakPointer<ConnectionBase>");
            return 0;
        }();

    m_thread->setObjectName("P2pMessageBus");
    connect(m_thread, &QThread::started,
        [this]()
        {
            if (!m_timer)
            {
                m_timer = new QTimer(this);
                connect(m_timer, &QTimer::timeout, this, [this]() { doPeriodicTasks(); });
                connect(this, &MessageBus::removeConnectionAsync, this, &MessageBus::removeConnection, Qt::QueuedConnection);
            }
            m_timer->start(kDefaultTimerInterval);
        });
    connect(m_thread, &QThread::finished,
        [this]()
        {
            m_timer->stop();
            onThreadStopped();
        });
}

MessageBus::~MessageBus()
{
    stop();
}

void MessageBus::dropConnections()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    dropConnectionsThreadUnsafe();
}

void MessageBus::dropConnectionsThreadUnsafe()
{
    NX_VERBOSE(this, "dropConnectionsThreadUnsafe() with %1 active and %2 outgoing connections",
        m_connections.size(), m_outgoingConnections.size());
    while (!m_connections.empty())
        removeConnectionUnsafe(m_connections.first());
    while(!m_outgoingConnections.empty())
        removeConnectionUnsafe(m_outgoingConnections.first());
    m_remoteUrls.clear();
    if (m_peers)
    {
        m_peers->clear();
        addOfflinePeersFromDb();
        emitPeerFoundLostSignals();
    }
}

void MessageBus::printTran(
    const P2pConnectionPtr& connection,
    const ec2::QnAbstractTransaction& tran,
    Connection::Direction direction) const
{
    auto localPeerName = peerName(peerId());
    QString msgName;
    QString directionName;
    if (direction == Connection::Direction::outgoing)
    {
        msgName = "Send";
        directionName = "--->";
    }
    else
    {
        msgName = "Got";
        directionName = "<---";
    }

    NX_VERBOSE(this, "%1 tran:\t %2 %3 %4. Command: %5. Seq: %6. timestamp: %7. Created by: %8(dbId=%9).",
        msgName,
        localPeerName,
        directionName,
        peerName(connection->remotePeer().id),
        toString(tran.command),
        tran.persistentInfo.sequence,
        tran.persistentInfo.timestamp,
        peerName(tran.peerID),
        tran.persistentInfo.dbID);
}

void MessageBus::start()
{
    m_localShortPeerInfo.encode(localPeer(), 0);
    m_peers.reset(new BidirectionRoutingInfo(localPeer()));
    addOfflinePeersFromDb();
    m_lastRuntimeInfo[localPeer()] = runtimeInfoManager()->localInfo().data;
    base_type::start();
    m_started = true;
}

void MessageBus::stop()
{
    m_started = false;

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_remoteUrls.clear();
    }

    dropConnections();
    base_type::stop();
}

void MessageBus::addOutgoingConnectionToPeer(
    const QnUuid& peer,
    nx::vms::api::PeerType peerType,
    const utils::Url &_url,
    std::optional<nx::network::http::Credentials> credentials,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    deleteRemoveUrlById(peer);

    nx::utils::Url url(_url);
    const auto patch = globalSettings()->isWebSocketEnabled()
        ? ConnectionBase::kWebsocketUrlPath : ConnectionBase::kHttpHandshakeUrlPath;
    if (peerType == nx::vms::api::PeerType::cloudServer)
    {
        url.setPath(nx::network::url::joinPath(
            kCloudPathPrefix.toStdString(), patch));
    }
    else
    {
        url.setPath(patch);
    }

    int pos = nx::utils::random::number((int) 0, (int) m_remoteUrls.size());
    m_remoteUrls.insert(
        m_remoteUrls.begin() + pos,
        RemoteConnection(peer, peerType, url, std::move(credentials), std::move(adapterFunc)));
    NX_VERBOSE(this, "peer %1 addOutgoingConnection to peer %2 type %3 using url \"%4\"",
        peerName(localPeer().id),
        peerName(peer), peerType, _url);
    executeInThread(m_thread, [this]() {doPeriodicTasks();});
}

void MessageBus::deleteRemoveUrlById(const QnUuid& id)
{
    for (decltype(m_remoteUrls.size()) i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_remoteUrls[i].peerId == id)
        {
            m_remoteUrls.erase(m_remoteUrls.begin() + i);
            break;
        }
    }
}

void MessageBus::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    deleteRemoveUrlById(id);

    m_outgoingConnections.remove(id);
    m_lastConnectionState.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == Connection::Direction::outgoing)
    {
        NX_VERBOSE(this,
            "peer %1 removeOutgoingConnection from peer %2 (active connection closed)",
            peerName(localPeer().id), peerName(id));
        removeConnectionUnsafe(itr.value());
    }
    else
    {
        NX_VERBOSE(this,
            "peer %1 removeOutgoingConnection from peer %2",
            peerName(localPeer().id), peerName(id));
    }
}

void MessageBus::updateOutgoingConnection(
    const QnUuid& id, nx::network::http::Credentials credentials)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    auto remoteUrl = std::find_if(m_remoteUrls.begin(), m_remoteUrls.end(),
        [id](const RemoteConnection& connection) { return connection.peerId == id; });

    if (remoteUrl == m_remoteUrls.end())
    {
        NX_DEBUG(this, "Can not find connection '%1'", id);
        return;
    }

    remoteUrl->credentials = credentials;
    removeConnectionUnsafe(m_connections.value(id));
}

void MessageBus::connectSignals(const P2pConnectionPtr& connection)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    connect(connection.data(), &Connection::stateChanged, this, &MessageBus::at_stateChanged, Qt::QueuedConnection);
    connect(connection.data(), &Connection::gotMessage, this, &MessageBus::at_gotMessage, Qt::QueuedConnection);
    connect(connection.data(), &Connection::allDataSent, this, &MessageBus::at_allDataSent, Qt::QueuedConnection);
}

void MessageBus::createOutgoingConnections(
    const QMap<PersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    int intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_intervals.outConnectionsInterval).count();
    if (m_outConnectionsTimer.isValid() && !m_outConnectionsTimer.hasExpired(intervalMs))
        return;
    m_outConnectionsTimer.restart();

    for (decltype(m_remoteUrls.size()) i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_outgoingConnections.size() >= m_miscData.newConnectionsAtOnce)
            return; //< wait a bit

        int pos = m_lastOutgoingIndex % m_remoteUrls.size();
        ++m_lastOutgoingIndex;

        RemoteConnection& remoteConnection = m_remoteUrls[pos];
        if (!m_connections.contains(remoteConnection.peerId) &&
            !m_outgoingConnections.contains(remoteConnection.peerId))
        {
            if (!needStartConnection(remoteConnection.peerId, currentSubscription))
            {
                continue;
            }

            // Connection to this peer have been closed recently

            nx::utils::erase_if(
                remoteConnection.disconnectTimes,
                [this](const auto& timer)
                {
                    return timer.hasExpired(m_intervals.remotePeerReconnectTimeout);
                });

            if (remoteConnection.disconnectTimes.size() >= 2
                || (remoteConnection.disconnectTimes.size() >= 1
                && remoteConnection.lastConnectionState == ConnectionBase::State::Unauthorized))
            {
                continue;
            }

            {
                // This check is redundant (can be omitted). But it reduce network race condition time.
                // So, it reduce frequency of in/out conflict and network traffic a bit.
                if (m_connectionGuardSharedState.contains(remoteConnection.peerId))
                    continue; //< incoming connection in progress
            }

            ConnectionLockGuard connectionLockGuard(
                peerId(),
                connectionGuardSharedState(),
                remoteConnection.peerId,
                ConnectionLockGuard::Direction::Outgoing);

            P2pConnectionPtr connection(new Connection(
                remoteConnection.adapterFunc,
                remoteConnection.credentials,
                systemContext(),
                remoteConnection.peerId,
                remoteConnection.peerType,
                localPeerEx(),
                remoteConnection.url,
                std::make_unique<ConnectionContext>(),
                std::move(connectionLockGuard),
                [this](const auto& remotePeer) { return validateRemotePeerData(remotePeer); }));
            connection->setMaxSendBufferSize(globalSettings()->maxP2pQueueSizeBytes());
            m_outgoingConnections.insert(remoteConnection.peerId, connection);
            ++m_connectionTries;
            connectSignals(connection);
            connection->startConnection();
        }
    }
}

bool MessageBus::isLocalConnection(const PersistentIdData& peer) const
{
    const auto connection = m_connections.value(peer.id);
    if (!connection)
        return false;
    const auto peerType = connection->remotePeer().peerType;
    return vms::api::PeerData::isClient(peerType)
        && peerType != vms::api::PeerType::videowallClient;
}

QMap<PersistentIdData, P2pConnectionPtr> MessageBus::getCurrentSubscription() const
{
    QMap<PersistentIdData, P2pConnectionPtr> result;
    for (auto itr = m_connections.cbegin(); itr != m_connections.cend(); ++itr)
    {
        auto context = this->context(itr.value());
        for (const auto peer: context->localSubscription)
            result[peer] = itr.value();
    }
    return result;
}

P2pConnectionPtr MessageBus::findConnectionById(const PersistentIdData& id) const
{
    P2pConnectionPtr result =  m_connections.value(id.id);
    return result && result->remotePeer().persistentId == id.persistentId ? result : P2pConnectionPtr();
}

bool MessageBus::needStartConnection(
    const PersistentIdData& peer,
    const QMap<PersistentIdData, P2pConnectionPtr>& currentSubscription) const
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    qint32 currentDistance = allPeerDistances.value(peer).minDistance();
    const auto& subscribedVia = currentSubscription.value(peer);
    auto result = currentDistance > m_miscData.maxDistanceToUseProxy
        || (subscribedVia && context(subscribedVia)->localSubscription.size() > m_miscData.maxSubscriptionToResubscribe);

    return result;
}

bool MessageBus::needStartConnection(
    const QnUuid& peerId,
    const QMap<PersistentIdData, P2pConnectionPtr>& currentSubscription) const
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;

    bool result = true;
    auto itr = allPeerDistances.lowerBound(PersistentIdData(peerId, QnUuid()));
    for (; itr != allPeerDistances.end() && itr.key().id == peerId; ++itr)
    {
        result &= needStartConnection(itr.key(), currentSubscription);
    }
    return result;
}

bool MessageBus::hasStartingConnections() const
{
    for (const auto& connection: m_connections)
    {
        if (connection->remotePeer().isServer())
        {
            auto data = context(connection);
            if (data->isLocalStarted && data->remotePeersMessage.isEmpty())
                return true;
        }
    }
    return false;
}

void MessageBus::MiscData::update()
{
    expectedConnections = std::max(
        (qsizetype) 1,
        std::max(owner->m_connections.size(), (qsizetype) owner->m_remoteUrls.size()));
    maxSubscriptionToResubscribe = qRound(std::sqrt(expectedConnections)) * 2;
    maxDistanceToUseProxy = std::max(2, int(std::sqrt(std::sqrt(expectedConnections))));
    newConnectionsAtOnce = std::max(1, int(qRound(std::sqrt(expectedConnections))) / 2);
}

void MessageBus::doPeriodicTasks()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (!isStarted())
    {
        NX_VERBOSE(this, "P2p message bus is not started. Skip periodic tasks");
        return;
    }

    createOutgoingConnections(getCurrentSubscription()); //< Open new connections.
}

vms::api::PeerData MessageBus::localPeer() const
{
    return runtimeInfoManager()->localInfo().data.peer;
}

vms::api::PeerDataEx MessageBus::localPeerEx() const
{
    nx::vms::api::PeerDataEx result;
    result.assign(localPeer());

    result.systemId = globalSettings()->localSystemId();
    result.cloudHost = nx::network::SocketGlobals::cloud().cloudHost().c_str();
    result.aliveUpdateIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>
        (globalSettings()->aliveUpdateInterval()).count();
    result.protoVersion = nx::vms::api::protocolVersion();
    return result;
}

void MessageBus::startReading(P2pConnectionPtr connection)
{
    context(connection)->encode(PersistentIdData(connection->remotePeer()), 0);
    connection->startReading();
}

void MessageBus::removeConnection(QWeakPointer<ConnectionBase> weakRef)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    removeConnectionUnsafe(weakRef);
}

void MessageBus::removeConnectionUnsafe(QWeakPointer<ConnectionBase> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    const auto& remotePeer = connection->remotePeer();
    NX_DEBUG(this,
        "Peer %1:%2 has closed connection to %3:%4",
        localPeer().peerType,
        peerName(localPeer().id),
        remotePeer.peerType,
        peerName(remotePeer.id));

    if (auto callback = context(connection)->onConnectionClosedCallback)
        callback();
    auto outgoingConnection = m_outgoingConnections.value(remotePeer.id);
    if (outgoingConnection == connection)
    {
        m_outgoingConnections.remove(remotePeer.id);
    }
    else
    {
        auto connectedConnection = m_connections.value(remotePeer.id);
        if (connectedConnection == connection)
        {
            m_peers->removePeer(connection->remotePeer());
            m_connections.remove(remotePeer.id);
        }
    }
    emitPeerFoundLostSignals();
    if (connection->state() == Connection::State::Unauthorized)
    {
        emitAsync(this, &MessageBus::remotePeerUnauthorized, remotePeer.id);
    }
    else if (connection->state() == Connection::State::forbidden)
    {
        emitAsync(this,
            &MessageBus::remotePeerForbidden,
            remotePeer.id,
            connection->lastErrorMessage());
    }
    else if (connection->state() == Connection::State::handshakeError)
    {
        emitAsync(this, &MessageBus::remotePeerHandshakeError, remotePeer.id);
    }
}

void MessageBus::at_stateChanged(
    QWeakPointer<ConnectionBase> weakRef,
    Connection::State /*state*/)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    NX_VERBOSE(this, "Connection [%1] state changed to [%2]", connection->remotePeer().id, connection->state());

    emit stateChanged(connection->remotePeer().id, toString(connection->state()));

    NX_MUTEX_LOCKER lock(&m_mutex);

    const auto& remoteId = connection->remotePeer().id;
    m_lastConnectionState[remoteId] = connection->state();

    switch (connection->state())
    {
        case Connection::State::Connected:
            if (connection->direction() == Connection::Direction::outgoing)
            {
                m_connections[remoteId] = connection;
                m_outgoingConnections.remove(remoteId);
                emitPeerFoundLostSignals();
                startReading(connection);
            }

            emit newDirectConnectionEstablished(connection.data());
            if (connection->remotePeer().peerType == PeerType::cloudServer)
                sendInitialDataToCloud(connection);

            break;
        case Connection::State::handshakeError:
        case Connection::State::Unauthorized:
        case Connection::State::forbidden:
        case Connection::State::Error:
            for (auto& removeUrlInfo: m_remoteUrls)
            {
                if (removeUrlInfo.peerId == remoteId)
                {
                    removeUrlInfo.disconnectTimes.push_back(nx::utils::ElapsedTimer());
                    removeUrlInfo.disconnectTimes.last().restart();
                    removeUrlInfo.lastConnectionState = connection->state();
                }
            }
            removeConnectionUnsafe(weakRef);
            break;
        default:
            break;
    }
}

void MessageBus::at_allDataSent(QWeakPointer<ConnectionBase> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;
    if (context(connection)->sendDataInProgress)
    {
        selectAndSendTransactions(
            connection,
            context(connection)->remoteSubscription,
            context(connection)->isRemoteSubscribedToAll);
    }
}

bool MessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    vms::api::TranState newSubscription,
    [[maybe_unused]] bool addImplicitData)
{
    context(connection)->sendDataInProgress = false;
    context(connection)->remoteSubscription = newSubscription;
    return true;
}

void MessageBus::at_gotMessage(
    QWeakPointer<ConnectionBase> weakRef,
    MessageType messageType,
    const nx::Buffer& payloadBuffer)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    const auto& payload = payloadBuffer.toByteArray();

    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!isStarted())
        return;

    if (m_connections.value(connection->remotePeer().id) != connection)
        return;

    if (connection->state() == Connection::State::Error)
        return; //< Connection has been closed
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this) &&
        messageType != MessageType::pushTransactionData &&
        messageType != MessageType::pushTransactionList)
    {
        auto localPeerName = peerName(peerId());

        NX_VERBOSE(
            this,
            "Got message:\t %1 <--- %2. Type: %3. Size=%4",
            localPeerName,
            peerName(connection->remotePeer().id),
            messageType,
            payload.size() + 1);
    }

    bool result = false;
    auto connectionContext = this->context(connection);
    switch (messageType)
    {
    case MessageType::start:
        connectionContext->isRemoteStarted = true;
        result = true;
        break;
    case MessageType::stop:
        connectionContext->sendDataInProgress = false;
        connectionContext->isRemoteStarted = false;
        connectionContext->remoteSubscription.values.clear();
        break;
    case MessageType::resolvePeerNumberRequest:
        result = handleResolvePeerNumberRequest(connection, payload);
        break;
    case MessageType::resolvePeerNumberResponse:
        result = handleResolvePeerNumberResponse(connection, payload);
        break;
    case MessageType::alivePeers:
        result = handlePeersMessage(connection, payload);
        break;
    case MessageType::subscribeForDataUpdates:
        result = handleSubscribeForDataUpdates(connection, payload);
        break;
    case MessageType::subscribeAll:
        result = handleSubscribeForAllDataUpdates(connection, payload);
        break;
    case MessageType::pushTransactionData:
        result = handlePushTransactionData(connection, payload, TransportHeader(), &lock);
        break;
    case MessageType::pushTransactionList:
        result = handlePushTransactionList(connection, payload, &lock);
        break;
    case MessageType::pushImpersistentUnicastTransaction:
        result = handleTransactionWithHeader(
            this,
            connection,
            payload,
            GotUnicastTransactionFuction(),
            &lock);
        break;
    case MessageType::pushImpersistentBroadcastTransaction:
        result = handlePushImpersistentBroadcastTransaction(connection, payload, &lock);
        break;
    default:
        NX_ASSERT(0, "Unknown message type: %1", (int) messageType);
        break;
    }
    if (!result)
        emit removeConnectionAsync(connection);
}

bool MessageBus::handlePushImpersistentBroadcastTransaction(
    const P2pConnectionPtr& connection,
    const QByteArray& payload,
    nx::Locker<nx::Mutex>* lock)
{
    return handleTransactionWithHeader(
        this,
        connection,
        payload,
        GotTransactionFuction(), lock);
}

bool MessageBus::handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<PeerNumberType> request = deserializeCompressedPeers(data, &success);
    if (!success)
        return false;

    QVector<PeerNumberResponseRecord> response;
    response.reserve(request.size());
    for (const auto& peer: request)
    {
        const auto fullPeerId = m_localShortPeerInfo.decode(peer);
        NX_ASSERT(!fullPeerId.isNull());
        response.push_back(PeerNumberResponseRecord(peer, fullPeerId));
    }

    auto responseData = serializeResolvePeerNumberResponse(response, 1);
    responseData.data()[0] = (quint8) MessageType::resolvePeerNumberResponse;
    connection->sendMessage(std::move(responseData));
    return true;
}

bool MessageBus::handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    auto records = deserializeResolvePeerNumberResponse(data, &success);
    if (!success)
        return false;

    auto connectionContext = context(connection);
    for (const auto& record: records)
    {
        connectionContext->encode(record, record.peerNumber);

        auto itr = std::find(
            connectionContext->awaitingNumbersToResolve.begin(),
            connectionContext->awaitingNumbersToResolve.end(),
            record.peerNumber);
        if (itr != connectionContext->awaitingNumbersToResolve.end())
            connectionContext->awaitingNumbersToResolve.erase(itr);
    }

    const QByteArray msg = context(connection)->remotePeersMessage;
    if (!msg.isEmpty())
        handlePeersMessage(connection, msg);
    return true;
}

bool MessageBus::handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data)
{
    NX_ASSERT(!data.isEmpty());

    bool success = false;
    auto peers = deserializePeersMessage(data, &success);
    if (!success)
        return false;

    m_lastPeerInfoTimer.restart();

    std::set<PeerNumberType> numbersToResolve;
    nx::utils::BitStreamReader reader((const quint8*)data.data(), data.size());
    for (const auto& peer: peers)
    {
        if (context(connection)->decode(peer.peerNumber).isNull())
            numbersToResolve.insert(peer.peerNumber);
        if (peer.firstVia != kUnknownPeerNumnber)
        {
            if (context(connection)->decode(peer.firstVia).isNull())
                numbersToResolve.insert(peer.firstVia);
        }
    }
    context(connection)->remotePeersMessage = data;

    if (numbersToResolve.empty())
    {
        m_peers->removePeer(connection->remotePeer());
        auto& shortPeers = context(connection)->shortPeerInfo;
        for (const auto& peer: peers)
        {
            int distance = peer.distance;
            if (distance < kMaxOnlineDistance)
            {
                ++distance;
                NX_ASSERT(distance != kMaxOnlineDistance);
            }

            auto firstVia = shortPeers.decode(peer.firstVia);
            if (firstVia.isNull())
            {
                // Direct connection to the target peer.
                firstVia = connection->localPeer();
            }
            else if (distance <= kMaxOnlineDistance)
            {
                const auto gatewayDistance = distanceTo(firstVia);
                if (gatewayDistance > kMaxOnlineDistance)
                    continue; //< Gateway is offline now.
                if (gatewayDistance < distance - 1)
                {
                    NX_VERBOSE(
                        this,
                        "Peer %1 ignores alivePeers record due to route loop. Distance to %2 is %3. Distance to gateway %4 is %5",
                        peerName(localPeer().id),
                        peerName(shortPeers.decode(peer.peerNumber).id),
                        distance,
                        peerName(firstVia.id),
                        gatewayDistance);
                    continue; //< Route loop detected.
                }
            }

            m_peers->addRecord(
                connection->remotePeer(),
                shortPeers.decode(peer.peerNumber),
                nx::p2p::RoutingRecord(distance, firstVia));
        }
        emitPeerFoundLostSignals();
        return true;
    }

    auto connectionContext = context(connection);
    QVector<PeerNumberType> moreNumbersToResolve;

    std::set_difference(
        numbersToResolve.begin(),
        numbersToResolve.end(),
        connectionContext->awaitingNumbersToResolve.begin(),
        connectionContext->awaitingNumbersToResolve.end(),
        std::inserter(moreNumbersToResolve, moreNumbersToResolve.begin()));

    if (moreNumbersToResolve.isEmpty())
        return true;

    for (const auto& number: moreNumbersToResolve)
        connectionContext->awaitingNumbersToResolve.push_back(number);
    std::sort(connectionContext->awaitingNumbersToResolve.begin(), connectionContext->awaitingNumbersToResolve.end());

    auto serializedData = serializeCompressedPeers(moreNumbersToResolve, 1);
    serializedData.data()[0] = (quint8) MessageType::resolvePeerNumberRequest;
    connection->sendMessage(std::move(serializedData));
    return true;
}

template<class T>
void MessageBus::sendTransactionImpl(
    const P2pConnectionPtr& connection,
    const ec2::QnTransaction<T>& srcTran,
    TransportHeader transportHeader)
{
    NX_ASSERT(srcTran.command != ApiCommand::NotDefined);

    const vms::api::PersistentIdData remotePeer(connection->remotePeer());

    if (transportHeader.via.find(remotePeer.id) != transportHeader.via.end())
    {
        NX_VERBOSE(this, "Ignore send transaction %1-->%2. Destination peer already handled transaction %3",
            peerName(localPeer().id), peerName(remotePeer.id), srcTran);
        return;
    }

    const auto& descriptor = ec2::getTransactionDescriptorByTransaction(srcTran);
    auto remoteAccess = descriptor->checkRemotePeerAccessFunc(
        systemContext(),
        connection.staticCast<Connection>()->userAccessData(),
        srcTran.params);
    if (remoteAccess == RemotePeerAccess::Forbidden)
    {
        NX_VERBOSE(this, "Permission check failed while sending transaction %1 to peer %2",
            srcTran, remotePeer.id);
        return;
    }

    const vms::api::PersistentIdData peerId(srcTran.peerID, srcTran.persistentInfo.dbID);
    const auto context = this->context(connection);

    ec2::QnTransaction<T> modifiedTran;
    if (connection->remotePeer().isClient())
    {
        modifiedTran = srcTran;
        if (ec2::amendOutputDataIfNeeded(
            connection.staticCast<Connection>()->userAccessData(),
            resourceAccessManager(),
            &modifiedTran.params))
        {
            // Make persistent info null in case if data has been amended. We don't want such
            // transactions be checked against serialized transactions cache.
            modifiedTran.persistentInfo = ec2::QnAbstractTransaction::PersistentInfo();
        }
    }
    const ec2::QnTransaction<T>& tran(connection->remotePeer().isClient() ? modifiedTran : srcTran);

    if (connection->remotePeer().isServer())
    {
        if (descriptor->isPersistent)
        {
            if (context->sendDataInProgress)
            {
                NX_VERBOSE(this, "Send to server %1 already in progress", peerName(remotePeer.id));
                return;
            }

            const auto reason = context->updateSequence(tran);
            if (reason != UpdateSequenceResult::ok)
            {
                NX_VERBOSE(this, "Server %1 skip transaction %2. Reason: %3", peerName(remotePeer.id), tran, toString(reason));
                return;
            }
        }
        else
        {
            if (!context->isRemotePeerSubscribedTo(tran.peerID))
            {
                NX_VERBOSE(this, "Peer %1 is not subscribed for %2", peerName(remotePeer.id), tran.peerID);
                return;
            }
        }
    }
    else if (remotePeer == peerId)
    {
        NX_VERBOSE(this, "Peer %1 is myself", peerName(remotePeer.id));
        return;
    }
    else if (connection->remotePeer().isCloudServer())
    {
        if (!descriptor->isPersistent)
        {
            NX_VERBOSE(this, "Cloud %1 is not iterested in non-persistent transactions", peerName(remotePeer.id));
            return;
        }

        if (context->sendDataInProgress)
        {
            NX_VERBOSE(this, "Send to cloud %1 already in progress", peerName(remotePeer.id));
            return;
        }

        const auto reason = context->updateSequence(tran);
        if (reason != UpdateSequenceResult::ok)
        {
            NX_VERBOSE(this, "Cloud %1 skip transaction %2. Reason: %3", peerName(remotePeer.id), tran, toString(reason));
            return;
        }
    }

    NX_ASSERT(!(remotePeer == peerId)); //< loop

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug, this))
    {
        if (connection->shouldTransactionBeSentToPeer(tran) == FilterResult::allow)
            printTran(connection, tran, Connection::Direction::outgoing);
    }

#if 0
    // TODO: it is improvement to handle persistent transactions in case of buffer overflow.
    if (descriptor->isPersistent && connection->maxSendBufferSize() > 0
        && connection->sendBufferSize() > connection->maxSendBufferSize() / 2)
    {
        // Switch from live mode to the DB selection mode.
        // Keep extra buffer size for non persistent transactions.
        context->sendDataInProgress = true;
        return;
    }
#endif

    switch (connection->remotePeer().dataFormat)
    {
    case Qn::SerializationFormat::json:
        connection->sendTransaction(
            tran,
            m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
        break;
    case Qn::SerializationFormat::ubjson:
        if (connection->remotePeer().isClient())
        {
            connection->sendTransaction(
                tran,
                m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
        }
        else if (descriptor->isPersistent)
        {
            connection->sendTransaction(
                tran,
                MessageType::pushTransactionData,
                m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
        }
        else
        {
            TransportHeader header(transportHeader);
            header.via.insert(localPeer().id);
            connection->sendTransaction(
                tran,
                MessageType::pushImpersistentBroadcastTransaction,
                m_ubjsonTranSerializer->serializedTransactionWithHeader(tran, header));
        }
        break;
    default:
        qWarning() << "Client has requested data in an unsupported format"
            << connection->remotePeer().dataFormat;
        break;
    }
}

void MessageBus::sendRuntimeData(
    const P2pConnectionPtr& connection,
    const QList<PersistentIdData>& peers)
{
    for (const auto& peer: peers)
    {
        auto runtimeInfoItr = m_lastRuntimeInfo.find(peer);
        if (runtimeInfoItr != m_lastRuntimeInfo.end())
        {
            QnTransaction<RuntimeData> tran(ApiCommand::runtimeInfoChanged, peer.id);
            tran.params = runtimeInfoItr.value();
            sendTransactionImpl(connection, tran, TransportHeader());
        }
    }
}

bool MessageBus::handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<SubscribeRecord> request = deserializeSubscribeRequest(data, &success);
    if (!success)
        return false;
    vms::api::TranState newSubscription;
    for (const auto& shortPeer : request)
    {
        const auto& id = m_localShortPeerInfo.decode(shortPeer.peer);
        NX_ASSERT(!id.isNull());
        newSubscription.values.insert(id, shortPeer.sequence);
    }
    context(connection)->isRemoteSubscribedToAll = false;

    // merge current and new subscription
    auto& oldSubscription = context(connection)->remoteSubscription;
    auto itrOldSubscription = oldSubscription.values.begin();
    QList<nx::vms::api::PersistentIdData> subscriptionDelta;
    for (auto itr = newSubscription.values.begin(); itr != newSubscription.values.end(); ++itr)
    {
        while (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() < itr.key())
            ++itrOldSubscription;

        if (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() == itr.key())
            itr.value() = std::max(itr.value(), itrOldSubscription.value());
        else
            subscriptionDelta.push_back(itr.key());
    }

    NX_ASSERT(!context(connection)->isRemotePeerSubscribedTo(connection->remotePeer()));
    if (context(connection)->sendDataInProgress)
    {
        context(connection)->remoteSubscription = newSubscription;
    }
    else
    {
        if (!selectAndSendTransactions(connection, std::move(newSubscription), false))
            return false;
    }
    sendRuntimeData(connection, subscriptionDelta);
    return true;
}

bool MessageBus::handleSubscribeForAllDataUpdates(
    const P2pConnectionPtr& connection,
    const QByteArray& data)
{
    NX_ASSERT(connection->remotePeer().peerType == PeerType::cloudServer);
    context(connection)->isRemoteSubscribedToAll = true;
    bool success = false;
    auto newSubscription = deserializeSubscribeAllRequest(data, &success);

    if (context(connection)->sendDataInProgress)
    {
        context(connection)->remoteSubscription = newSubscription;
    }
    else
    {
        if (!selectAndSendTransactions(connection, std::move(newSubscription), true))
            return false;
    }
    return true;
}

void MessageBus::updateOfflineDistance(
    const P2pConnectionPtr& connection,
    const PersistentIdData& to,
    int sequence)
{
    const qint32 offlineDistance = kMaxDistance - sequence;

    const auto updateDistance =
        [&](const vms::api::PeerData& via)
        {
            const qint32 toDistance = m_peers->alivePeers[via].distanceTo(to);
            if (offlineDistance < toDistance)
            {
                nx::p2p::RoutingRecord record(offlineDistance);
                m_peers->addRecord(via, to, record);
            }
        };
    updateDistance(connection->remotePeer());
    updateDistance(localPeer());
}

void MessageBus::cleanupRuntimeInfo(const PersistentIdData& peer)
{
    // If media server was restarted it could get new DB.
    // At this case we would have two records in m_lastRuntimeInfo list.
    // As soon as 'old' record will be removed resend runtime notification
    // to make sure we emit later runtime version.
    m_lastRuntimeInfo.remove(peer);
    auto itr = m_lastRuntimeInfo.lowerBound(PersistentIdData(peer.id, QnUuid()));
    if (itr != m_lastRuntimeInfo.end() && itr.key().id == peer.id)
    {
        if (m_handler)
        {
            QnTransaction<RuntimeData> tran(ApiCommand::runtimeInfoChanged, peer.id);
            tran.params = itr.value();
            m_handler->triggerNotification(tran, NotificationSource::Remote);
        }
    }
}

template <>
void MessageBus::gotTransaction(
    const QnTransaction<nx::vms::api::UpdateSequenceData> &tran,
    const P2pConnectionPtr& connection,
    [[maybe_unused]] const TransportHeader& transportHeader,
    nx::Locker<nx::Mutex>* /*lock*/)
{
    PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    updateOfflineDistance(connection, peerId, tran.persistentInfo.sequence);
}

template <>
void MessageBus::gotTransaction(
    const QnTransaction<nx::vms::api::RuntimeData>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader,
    nx::Locker<nx::Mutex>* lock)
{
    if (localPeer().isServer() && !isSubscribedTo(connection->remotePeer()))
        return; // Ignore deprecated transaction.

    PersistentIdData peerId(tran.params.peer);

    if (m_lastRuntimeInfo.value(peerId) == tran.params)
    {
        NX_VERBOSE(this, "Peer %1 ignore runtimeInfo from peer %2 because it is already processed",
            peerName(localPeer().id), peerName(peerId.id));
        sendTransaction(tran, transportHeader); //< Proxy transaction.
        return; //< Already processed on the local peer.
    }

    if (peerId.id == localPeer().id)
    {
        NX_DEBUG(this, "Ignore deprecated runtime information about himself");
        return;
    }

    m_lastRuntimeInfo[peerId] = tran.params;

    if (m_handler)
    {
        nx::Unlocker<nx::Mutex> unlock(lock);
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    }
    emitPeerFoundLostSignals();
    // Proxy transaction to subscribed peers
    sendTransaction(tran, transportHeader);
}

template <class T>
void MessageBus::gotTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& /*connection*/,
    const TransportHeader& /*transportHeader*/,
    nx::Locker<nx::Mutex>* lock)
{
    if (m_handler)
    {
        nx::Unlocker<nx::Mutex> unlock(lock);
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    }
}

template <class T>
void MessageBus::gotUnicastTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& header,
    nx::Locker<nx::Mutex>* lock)
{
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
        printTran(connection, tran, Connection::Direction::incoming);

    std::set<QnUuid> unprocessedPeers;
    for (auto& peer: header.dstPeers)
    {
        if (peer == localPeer().id)
        {
            if (m_handler)
            {
                nx::Unlocker<nx::Mutex> unlock(lock);
                m_handler->triggerNotification(tran, NotificationSource::Remote);
            }
            continue;
        }
        unprocessedPeers.insert(peer);
    }

    // Split dstPeers by connection.
    QMap<P2pConnectionPtr, TransportHeader> dstByConnection;
    for (const auto& dstPeer: unprocessedPeers)
    {
        QVector<PersistentIdData> via;
        int distance = kMaxDistance;
        QnUuid dstPeerId = routeToPeerVia(dstPeer, &distance, /*address*/ nullptr);
        if (distance > kMaxOnlineDistance || dstPeerId.isNull())
        {
            NX_WARNING(this, nx::format("Drop unicast transaction because no route found"));
            continue;
        }
        if (const auto& dstConnection = m_connections.value(dstPeerId))
            dstByConnection[dstConnection].dstPeers.push_back(dstPeer);
        else
            NX_ASSERT(0, nx::format("Drop unicast transaction. Can't find connection to route"));
    }
    for (TransportHeader& newHeader: dstByConnection)
        newHeader.via = header.via;
    sendUnicastTransactionImpl(tran, dstByConnection);
}

bool MessageBus::handlePushTransactionList(
    const P2pConnectionPtr& connection,
    const QByteArray& data,
    nx::Locker<nx::Mutex>* lock)
{
    if (data.isEmpty())
    {
        context(connection)->recvDataInProgress = false;
        return true; //< eof pushTranList reached
    }
    bool success = false;
    auto tranList =  deserializeTransactionList(data, &success);
    if (!success)
        return false;
    for (const auto& transaction: tranList)
    {
        if (!handlePushTransactionData(connection, transaction, TransportHeader(), lock))
            return false;
    }
    return true;
}

bool MessageBus::handlePushTransactionData(
    const P2pConnectionPtr& connection,
    const QByteArray& serializedTran,
    const TransportHeader& header,
    nx::Locker<nx::Mutex>* lock)
{
    // Workaround for compatibility with server 3.1/3.2 with p2p mode on
    // It could send subscribeForDataUpdates binary message among json data.
    // TODO: we have to remove this checking in 4.0 because it is fixed on server side.
    if (localPeer().dataFormat == Qn::SerializationFormat::json && !serializedTran.isEmpty()
        && serializedTran[0] == (quint8)MessageType::subscribeForDataUpdates)
    {
        return true; //< Ignore binary message
    }

    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->localPeer().dataFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, connection, header, lock),
        [](Qn::SerializationFormat, const QnAbstractTransaction&, const QByteArray&) { return false; });
}

bool MessageBus::isSubscribedTo(const PersistentIdData& peer) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (PersistentIdData(localPeer()) == peer)
        return true;
    for (const auto& connection: m_connections)
    {
        if (connection->state() != Connection::State::Connected)
            continue;
        if (context(connection)->isLocalPeerSubscribedTo(peer))
            return true;
    }
    return false;
}

qint32 MessageBus::distanceTo(const PersistentIdData& peer) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (PersistentIdData(localPeer()) == peer)
        return 0;
    return m_peers->distanceTo(peer);
}

QMap<QnUuid, P2pConnectionPtr> MessageBus::connections() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_connections;
}

int MessageBus::connectionTries() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_connectionTries;
}

QSet<QnUuid> MessageBus::directlyConnectedClientPeers() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    QSet<QnUuid> result;
    for (const auto& connection: m_connections)
    {
        if (connection->remotePeer().isClient())
            result.insert(connection->remotePeer().id);
    }
    return result;
}

QSet<QnUuid> MessageBus::directlyConnectedServerPeers() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return nx::utils::toQSet(m_connections.keys());
}

QnUuid MessageBus::routeToPeerVia(
    const QnUuid& peerId, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (knownPeerAddress)
    {
        *knownPeerAddress = nx::network::SocketAddress();
        for (const auto& peer: m_remoteUrls)
        {
            if (peerId == peer.peerId)
            {
                *knownPeerAddress = nx::network::url::getEndpoint(peer.url);
                break;
            }
        }
    }

    if (localPeer().id == peerId)
    {
        *distance = 0;
        return QnUuid();
    }

    RoutingInfo via;
    *distance = m_peers->distanceTo(peerId, &via);
    return via.isEmpty() ? QnUuid() : via.begin().key().id;
}

int MessageBus::distanceToPeer(const QnUuid& peerId) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (localPeer().id == peerId)
        return 0;
    return m_peers->distanceTo(peerId);
}

ConnectionContext* MessageBus::context(const P2pConnectionPtr& connection)
{
    return static_cast<ConnectionContext*> (connection->opaqueObject());
}

ConnectionInfos MessageBus::connectionInfos() const
{
    ConnectionInfos result;
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto remoteUrls = m_remoteUrls;

    auto addDataToResult = [&](const QMap<QnUuid, P2pConnectionPtr>& connections)
    {
        for (const auto& connection: connections)
        {
            const auto& context = this->context(connection);

            QnTransportConnectionInfo info;
            info.url = connection->remoteAddr();
            info.state = toString(connection->state());
            info.isIncoming = connection->direction() == Connection::Direction::incoming;
            info.remotePeerId = connection->remotePeer().id;
            info.remotePeerDbId = connection->remotePeer().persistentId;
            info.isStarted = context->isLocalStarted;
            info.peerType = connection->remotePeer().peerType;
            auto vLocal = context->localSubscription;
            info.subscribedTo = std::vector<PersistentIdData>(vLocal.begin(), vLocal.end());
            auto vRemote = context->remoteSubscription.values.keys().toVector();
            info.subscribedFrom = std::vector<PersistentIdData>(vRemote.begin(), vRemote.end());

            // Has got peerInfo message from remote server. Open new connection is blocked
            // unless server have opening connection that hasn't got this answer from remote server
            // yet. It caused by optimization to do not open all connections to each other.
            info.gotPeerInfo = !context->remotePeersMessage.isEmpty();

            result.connections.push_back(info);
        }

        remoteUrls.erase(std::remove_if(remoteUrls.begin(), remoteUrls.end(),
            [&](const RemoteConnection& data)
            {
                return connections.contains(data.peerId);
            }),
            remoteUrls.end());
    };

    addDataToResult(m_connections);
    addDataToResult(m_outgoingConnections);

    for (const auto& peer: remoteUrls)
    {
        QnTransportConnectionInfo info;
        info.url = peer.url;
        info.state = "Not opened";
        info.isIncoming = false;
        info.remotePeerId = peer.peerId;
        result.connections.push_back(info);
    }

    for (auto& record: result.connections)
        record.previousState = toString(m_lastConnectionState.value(record.remotePeerId));
    result.idData = localPeer();
    return result;
}

void MessageBus::emitPeerFoundLostSignals()
{
    std::set<vms::api::PeerData> newAlivePeers;

    for (const auto& connection: m_connections)
        newAlivePeers.insert(connection->remotePeer());

    for (auto itr = m_peers->allPeerDistances.constBegin(); itr != m_peers->allPeerDistances.constEnd(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == PersistentIdData(localPeer()))
            continue;
        if (itr->minDistance() < kMaxOnlineDistance)
        {
            const auto peerData = m_lastRuntimeInfo.value(peer);
            if (!peerData.peer.isNull())
                newAlivePeers.insert(peerData.peer);
        }
    }

    std::vector<vms::api::PeerData> newPeers;
    std::set_difference(
        newAlivePeers.begin(),
        newAlivePeers.end(),
        m_lastAlivePeers.begin(),
        m_lastAlivePeers.end(),
        std::inserter(newPeers, newPeers.begin()));

    std::vector<vms::api::PeerData> lostPeers;
    std::set_difference(
        m_lastAlivePeers.begin(),
        m_lastAlivePeers.end(),
        newAlivePeers.begin(),
        newAlivePeers.end(),
        std::inserter(lostPeers, lostPeers.begin()));

    for (const auto& peer: newPeers)
    {
        NX_DEBUG(this,
            "Peer %1 has found peer %2",
            peerName(localPeer().id),
            peerName(peer.id));
        emitAsync(this, &MessageBus::peerFound, peer.id, peer.peerType);
    }

    for (const auto& peer: lostPeers)
    {
        cleanupRuntimeInfo(peer);

        vms::api::PeerData samePeer(PersistentIdData(peer.id, QnUuid()), peer.peerType);
        auto samePeerItr = newAlivePeers.lower_bound(samePeer);
        bool hasSimilarPeer = samePeerItr != newAlivePeers.end() && samePeerItr->id == peer.id;
        if (!hasSimilarPeer)
        {
            NX_DEBUG(this,
                "Peer %1 has lost peer %2",
                peerName(localPeer().id),
                peerName(peer.id));
            emitAsync(this, &MessageBus::peerLost, peer.id, peer.peerType);
            sendRuntimeInfoRemovedToClients(peer.id);
        }
    }

    m_lastAlivePeers = newAlivePeers;
}

void MessageBus::sendRuntimeInfoRemovedToClients(const QnUuid& id)
{
    QnTransaction<nx::vms::api::IdData> tran(ApiCommand::runtimeInfoRemoved, id);
    tran.params.id = id;
    for (const auto& connection: m_connections)
    {
        if (connection->remotePeer().isClient())
            sendTransactionImpl(connection, tran, TransportHeader());
    }
}

void MessageBus::setDelayIntervals(const DelayIntervals& intervals)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_intervals = intervals;
    executeInThread(
        m_thread, [this]()
        {
            m_timer->setInterval(std::min(m_intervals.minInterval(), kDefaultTimerInterval));
        });
}

MessageBus::DelayIntervals MessageBus::delayIntervals() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_intervals;
}

QMap<PersistentIdData, RuntimeData> MessageBus::runtimeInfo() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lastRuntimeInfo;
}

void MessageBus::sendInitialDataToCloud([[maybe_unused]] const P2pConnectionPtr& connection)
{
    NX_ASSERT(0, "Not implemented");
}

bool MessageBus::validateRemotePeerData(const vms::api::PeerDataEx& /*remotePeer*/)
{
    return true;
}

template <>
void MessageBus::sendTransaction(const ec2::QnTransaction<vms::api::RuntimeData>& tran)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    vms::api::PersistentIdData peerId(tran.params.peer);
    m_lastRuntimeInfo[peerId] = tran.params;
    for (const auto& connection: m_connections)
        sendTransactionImpl(connection, tran, TransportHeader());
}

template<class T>
void MessageBus::sendTransaction(const ec2::QnTransaction<T>& tran, const TransportHeader& header)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& connection: m_connections)
        sendTransactionImpl(connection, tran, header);
}

template<class T>
void MessageBus::sendTransaction(const ec2::QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers)
{
    NX_ASSERT(tran.command != ApiCommand::NotDefined);
    NX_MUTEX_LOCKER lock(&m_mutex);
    sendUnicastTransaction(tran, dstPeers);
}

template<class T>
void MessageBus::sendTransaction(const ec2::QnTransaction<T>& tran)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    for (const auto& connection: m_connections)
        sendTransactionImpl(connection, tran, TransportHeader());
}

template<class T>
void MessageBus::sendUnicastTransaction(const QnTransaction<T>& tran, const vms::api::PeerSet& dstPeers)
{
    QMap<P2pConnectionPtr, TransportHeader> dstByConnection;

    // split dstPeers by connection
    for (const auto& peer : dstPeers)
    {
        qint32 distance = kMaxDistance;
        auto dstPeer = routeToPeerVia(peer, &distance, /*address*/ nullptr);
        if (const auto& connection = m_connections.value(dstPeer))
            dstByConnection[connection].dstPeers.push_back(peer);
    }
    sendUnicastTransactionImpl(tran, dstByConnection);
}

template<class T>
void MessageBus::sendUnicastTransactionImpl(
    const QnTransaction<T>& tran,
    const QMap<P2pConnectionPtr, TransportHeader>& dstByConnection)
{
    for (auto itr = dstByConnection.begin(); itr != dstByConnection.end(); ++itr)
    {
        const auto& connection = itr.key();
        TransportHeader transportHeader = itr.value();

        if (transportHeader.via.find(connection->remotePeer().id) != transportHeader.via.end())
            continue; //< Already processed by remote peer

        if (connection->remotePeer().isClient())
        {
            if (transportHeader.dstPeers.size() != 1
                || transportHeader.dstPeers[0] != connection->remotePeer().id)
            {
                NX_ASSERT(0, nx::format("Unicast transaction routing error. "
                    "Transaction %1 skipped. remotePeer: %2")
                    .args(tran.command, connection->remotePeer().id));
                return;
            }
            switch (connection->remotePeer().dataFormat)
            {
            case Qn::SerializationFormat::json:
                connection->sendTransaction(
                    tran,
                    m_jsonTranSerializer->serializedTransactionWithoutHeader(tran) + QByteArray("\r\n"));
                break;
            case Qn::SerializationFormat::ubjson:
                connection->sendTransaction(
                    tran,
                    m_ubjsonTranSerializer->serializedTransactionWithoutHeader(tran));
                break;
            default:
                NX_WARNING(this,
                    nx::format("Client has requested data in an unsupported format %1")
                    .arg(connection->remotePeer().dataFormat));
                break;
            }
        }
        else
        {
            switch (connection->remotePeer().dataFormat)
            {
            case Qn::SerializationFormat::ubjson:
                transportHeader.via.insert(localPeer().id);
                connection->sendTransaction(
                    tran,
                    MessageType::pushImpersistentUnicastTransaction,
                    m_ubjsonTranSerializer->serializedTransactionWithHeader(tran, transportHeader));
                break;
            default:
                NX_WARNING(this, nx::format("Server has requested data in an unsupported format %1")
                    .arg(connection->remotePeer().dataFormat));
                break;
            }
        }
    }
}

#define INSTANTIATE(unused1, unused2, T) \
template void MessageBus::sendTransaction(const ec2::QnTransaction<T>&); \
template void MessageBus::sendTransaction(const ec2::QnTransaction<T>&, const TransportHeader&); \
template void MessageBus::sendTransaction(const ec2::QnTransaction<T>&, const vms::api::PeerSet&); \
template void MessageBus::sendTransactionImpl(const P2pConnectionPtr&, \
    const ec2::QnTransaction<T>&, TransportHeader);

#include <transaction_types.i>
BOOST_PP_SEQ_FOR_EACH(INSTANTIATE, _, TransactionDataTypes (UserDataEx))

} // namespace p2p
} // namespace nx
