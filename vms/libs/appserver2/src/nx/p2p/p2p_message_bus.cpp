#include "p2p_message_bus.h"

#include <common/common_module.h>
#include <utils/media/bitStream.h>
#include <utils/common/synctime.h>
#include "ec_connection_notification_manager.h"
#include <transaction/transaction_message_bus_priv.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/math/math.h>
#include <api/runtime_info_manager.h>

#include <nx/cloud/db/api/ec2_request_paths.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cmath.h>
#include <nx/utils/random.h>
#include <nx/vms/api/data/update_sequence_data.h>

namespace nx {
namespace p2p {

const QString MessageBus::kCloudPathPrefix(lit("/cdb"));

using namespace ec2;
using namespace vms::api;

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        MessageBus *bus,
        const QnTransaction<T>& transaction,
        const P2pConnectionPtr& connection,
        const TransportHeader& transportHeader) const
    {
        bus->gotTransaction(transaction, connection, transportHeader);
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
        const TransportHeader& transportHeader) const
    {
        bus->gotUnicastTransaction(transaction, connection, transportHeader);
    }
};

// ---------------------- P2pMessageBus --------------

MessageBus::MessageBus(
    vms::api::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)
:
    TransactionMessageBusBase(peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer),
    m_miscData(this)
{
    qRegisterMetaType<MessageType>();
    qRegisterMetaType<ConnectionBase::State>("ConnectionBase::State");
    qRegisterMetaType<P2pConnectionPtr>("P2pConnectionPtr");
    qRegisterMetaType<QWeakPointer<ConnectionBase>>("QWeakPointer<ConnectionBase>");

    m_thread->setObjectName("P2pMessageBus");
    connect(m_thread, &QThread::started,
        [this, peerType]()
        {
            if (!m_timer)
            {
                m_timer = new QTimer(this);
                connect(m_timer, &QTimer::timeout, this, [this]() { doPeriodicTasks(); });
                connect(this, &MessageBus::removeConnectionAsync, this, &MessageBus::removeConnection, Qt::QueuedConnection);
            }
            m_timer->start(500);
        });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });
}

MessageBus::~MessageBus()
{
    stop();
}

void MessageBus::dropConnections()
{
    QnMutexLocker lock(&m_mutex);
    dropConnectionsThreadUnsafe();
}

void MessageBus::dropConnectionsThreadUnsafe()
{
    for (const auto& connection: m_connections)
        removeConnectionAsync(connection);
    for (const auto& connection: m_outgoingConnections)
        removeConnectionAsync(connection);
    m_remoteUrls.clear();
    if (m_peers)
    {
        m_peers->clear();
        emitPeerFoundLostSignals();
    }
}

void MessageBus::printTran(
    const P2pConnectionPtr& connection,
    const ec2::QnAbstractTransaction& tran,
    Connection::Direction direction) const
{
    auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
    auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);
    QString msgName;
    QString directionName;
    if (direction == Connection::Direction::outgoing)
    {
        msgName = lit("Send");
        directionName = lit("--->");
    }
    else
    {
        msgName = lit("Got");
        directionName = lit("<---");
    }

    NX_VERBOSE(this, lm("%1 tran:\t %2 %3 %4. Command: %5. Seq=%6. Created by %7").args(
        msgName,
        localPeerName,
        directionName,
        remotePeerName,
        toString(tran.command),
        tran.persistentInfo.sequence,
        qnStaticCommon->moduleDisplayName(tran.peerID)));
}

void MessageBus::start()
{
    m_localShortPeerInfo.encode(localPeer(), 0);
    m_peers.reset(new BidirectionRoutingInfo(localPeer()));
    addOfflinePeersFromDb();
    m_lastRuntimeInfo[localPeer()] = commonModule()->runtimeInfoManager()->localInfo().data;
    base_type::start();
    m_started = true;
}

void MessageBus::stop()
{
    m_started = false;

    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUrls.clear();
    }

    dropConnections();
    base_type::stop();
}

void MessageBus::addOutgoingConnectionToPeer(
    const QnUuid& peer,
    nx::vms::api::PeerType peerType,
    const utils::Url &_url)
{
    QnMutexLocker lock(&m_mutex);
    deleteRemoveUrlById(peer);

    nx::utils::Url url(_url);
    if (peerType == nx::vms::api::PeerType::cloudServer)
    {
        url.setPath(nx::network::url::joinPath(
            kCloudPathPrefix.toStdString(), ConnectionBase::kWebsocketUrlPath.toStdString()).c_str());
    }
    else
    {
        url.setPath(ConnectionBase::kWebsocketUrlPath);
    }

    int pos = nx::utils::random::number((int) 0, (int) m_remoteUrls.size());
    m_remoteUrls.insert(m_remoteUrls.begin() + pos, RemoteConnection(peer, url));
}

void MessageBus::deleteRemoveUrlById(const QnUuid& id)
{
    for (int i = 0; i < m_remoteUrls.size(); ++i)
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
    QnMutexLocker lock(&m_mutex);
    deleteRemoveUrlById(id);

    m_outgoingConnections.remove(id);
    m_lastConnectionState.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == Connection::Direction::outgoing)
        removeConnectionAsync(itr.value());
}

void MessageBus::connectSignals(const P2pConnectionPtr& connection)
{
    QnMutexLocker lock(&m_mutex);
    connect(connection.data(), &Connection::stateChanged, this, &MessageBus::at_stateChanged, Qt::QueuedConnection);
    connect(connection.data(), &Connection::gotMessage, this, &MessageBus::at_gotMessage, Qt::QueuedConnection);
    connect(connection.data(), &Connection::allDataSent, this, &MessageBus::at_allDataSent, Qt::QueuedConnection);
}

void MessageBus::createOutgoingConnections(
    const QMap<PersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    if (hasStartingConnections())
        return;

    if (commonModule()->isStandAloneMode())
        return;

    int intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        m_intervals.outConnectionsInterval).count();
    if (m_outConnectionsTimer.isValid() && !m_outConnectionsTimer.hasExpired(intervalMs))
        return;
    m_outConnectionsTimer.restart();

    auto itr = m_remoteUrls.begin();

    for (int i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_outgoingConnections.size() >= m_miscData.newConnectionsAtOnce)
            return; //< wait a bit

        int pos = m_lastOutgoingIndex % m_remoteUrls.size();
        ++m_lastOutgoingIndex;

        const RemoteConnection& remoteConnection = m_remoteUrls[pos];
        if (!m_connections.contains(remoteConnection.peerId) &&
            !m_outgoingConnections.contains(remoteConnection.peerId))
        {
            if (!needStartConnection(remoteConnection.peerId, currentSubscription))
            {
                continue;
            }

            {
                // This check is redundant (can be ommited). But it reduce network race condition time.
                // So, it reduce frequency of in/out conflict and network traffic a bit.
                if (m_connectionGuardSharedState.contains(remoteConnection.peerId))
                    continue; //< incoming connection in progress
            }

            ConnectionLockGuard connectionLockGuard(
                commonModule()->moduleGUID(),
                connectionGuardSharedState(),
                remoteConnection.peerId,
                ConnectionLockGuard::Direction::Outgoing);

            P2pConnectionPtr connection(new Connection(
                commonModule(),
                remoteConnection.peerId,
                localPeerEx(),
                remoteConnection.url,
                std::make_unique<ConnectionContext>(),
                std::move(connectionLockGuard),
                [this](const auto& remotePeer) { return validateRemotePeerData(remotePeer); }));
            m_outgoingConnections.insert(remoteConnection.peerId, connection);
            ++m_connectionTries;
            connectSignals(connection);
            connection->startConnection();
        }
    }
}

void MessageBus::printPeersMessage()
{
    QList<QString> records;

    for (auto itr = m_peers->allPeerDistances.constBegin(); itr != m_peers->allPeerDistances.constEnd(); ++itr)
    {
        const auto& peer = itr.value();

        RoutingInfo outViaList;
        qint32 minDistance = peer.minDistance(&outViaList);
        if (minDistance == kMaxDistance)
            continue;
        m_localShortPeerInfo.encode(itr.key());

        QStringList outViaListStr;
        for (const auto& peer: outViaList.keys())
            outViaListStr << qnStaticCommon->moduleDisplayName(peer.id);

        records << lit("\t\t\t\t\t To:  %1(dbId=%2). Distance: %3 (via %4)")
            .arg(qnStaticCommon->moduleDisplayName(itr.key().id))
            .arg(itr.key().persistentId.toString())
            .arg(minDistance)
            .arg(outViaListStr.join(","));
    }

    NX_VERBOSE(
        this,
        lit("Peer %1 records:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(records.join("\n")));
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
    expectedConnections = std::max(1, std::max(owner->m_connections.size(), (int) owner->m_remoteUrls.size()));
    maxSubscriptionToResubscribe = qRound(std::sqrt(expectedConnections)) * 2;
    maxDistanceToUseProxy = std::max(2, int(std::sqrt(std::sqrt(expectedConnections))));
    newConnectionsAtOnce = std::max(1, int(qRound(std::sqrt(expectedConnections))) / 2);
}

void MessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);
    createOutgoingConnections(getCurrentSubscription()); //< Open new connections.
}

vms::api::PeerData MessageBus::localPeer() const
{
    return vms::api::PeerData(
        commonModule()->moduleGUID(),
        commonModule()->runningInstanceGUID(),
        commonModule()->dbId(),
        m_localPeerType);
}

vms::api::PeerDataEx MessageBus::localPeerEx() const
{
    using namespace vms::api;
    PeerDataEx result;
    result.id = commonModule()->moduleGUID();
    result.persistentId = commonModule()->dbId();
    result.instanceId = commonModule()->runningInstanceGUID();
    result.systemId = commonModule()->globalSettings()->localSystemId();
    result.peerType = m_localPeerType;
    result.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
    result.identityTime = commonModule()->systemIdentityTime();
    result.aliveUpdateIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>
        (commonModule()->globalSettings()->aliveUpdateInterval()).count();
    result.protoVersion = nx_ec::EC2_PROTO_VERSION;
    result.dataFormat =
        m_localPeerType == PeerType::mobileClient ?  Qn::JsonFormat : Qn::UbjsonFormat;
    return result;
}

void MessageBus::startReading(P2pConnectionPtr connection)
{
    context(connection)->encode(PersistentIdData(connection->remotePeer()), 0);
    connection->startReading();
}

void MessageBus::removeConnection(QWeakPointer<ConnectionBase> weakRef)
{
    QnMutexLocker lock(&m_mutex);
    removeConnectionUnsafe(weakRef);
}

void MessageBus::removeConnectionUnsafe(QWeakPointer<ConnectionBase> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    const auto& remoteId = connection->remotePeer().id;
    NX_DEBUG(
        this,
        lit("Peer %1 has closed connection to %2")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)));

    if (auto callback = context(connection)->onConnectionClosedCallback)
        callback();
    auto outgoingConnection = m_outgoingConnections.value(remoteId);
    if (outgoingConnection == connection)
    {
        m_outgoingConnections.remove(remoteId);
    }
    else
    {
        auto connectedConnection = m_connections.value(remoteId);
        if (connectedConnection == connection)
        {
            m_peers->removePeer(connection->remotePeer());
            m_connections.remove(remoteId);
        }
    }
    emitPeerFoundLostSignals();
    if (connection->state() == Connection::State::Unauthorized)
        emit remotePeerUnauthorized(connection->remotePeer().id);
}

void MessageBus::at_stateChanged(
    QWeakPointer<ConnectionBase> weakRef,
    Connection::State /*state*/)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);

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
        case Connection::State::Unauthorized:
        case Connection::State::Error:
        {
            removeConnectionUnsafe(weakRef);
            break;
        }
        default:
            break;
    }
}

void MessageBus::at_allDataSent(QWeakPointer<ConnectionBase> weakRef)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;
    if (context(connection)->sendDataInProgress)
    {
        selectAndSendTransactions(
            connection,
            context(connection)->remoteSubscription,
            context(connection)->remoteAddImplicitData);
    }
}

bool MessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    vms::api::TranState newSubscription,
    bool addImplicitData)
{
    context(connection)->sendDataInProgress = false;
    context(connection)->remoteSubscription = newSubscription;
    return true;
}

void MessageBus::at_gotMessage(
    QWeakPointer<ConnectionBase> weakRef,
    MessageType messageType,
    const nx::Buffer& payload)
{
    P2pConnectionPtr connection = weakRef.toStrongRef();
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);

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
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);

        NX_VERBOSE(
            this,
            lit("Got message:\t %1 <--- %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(payload.size() + 1));
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
        result = handlePushTransactionData(connection, payload, TransportHeader());
        break;
    case MessageType::pushTransactionList:
        result = handlePushTransactionList(connection, payload);
        break;
    case MessageType::pushImpersistentUnicastTransaction:
        result = handleTransactionWithHeader(
            this,
            connection,
            payload,
            GotUnicastTransactionFuction());
        break;
    case MessageType::pushImpersistentBroadcastTransaction:
        result = handlePushImpersistentBroadcastTransaction(connection, payload);
        break;
    default:
        NX_ASSERT(0, lm("Unknown message type").arg((int)messageType));
        break;
    }
    if (!result)
        removeConnectionAsync(connection);
}

bool MessageBus::handlePushImpersistentBroadcastTransaction(
    const P2pConnectionPtr& connection,
    const QByteArray& payload)
{
    return handleTransactionWithHeader(
        this,
        connection,
        payload,
        GotTransactionFuction());
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
    connection->sendMessage(responseData);
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
    BitStreamReader reader((const quint8*)data.data(), data.size());
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
                        lm("Peer %1 ignores alivePeers record due to route loop. Distance to %2 is %3. Distance to gateway %4 is %5")
                        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                        .arg(qnStaticCommon->moduleDisplayName(shortPeers.decode(peer.peerNumber).id))
                        .arg(distance)
                        .arg(qnStaticCommon->moduleDisplayName(firstVia.id))
                        .arg(gatewayDistance));
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
    connection->sendMessage(serializedData);
    return true;
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
    context(connection)->remoteAddImplicitData = false;

    // merge current and new subscription
    auto& oldSubscription = context(connection)->remoteSubscription;
    auto itrOldSubscription = oldSubscription.values.begin();
    for (auto itr = newSubscription.values.begin(); itr != newSubscription.values.end(); ++itr)
    {
        while (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() < itr.key())
            ++itrOldSubscription;

        if (itrOldSubscription != oldSubscription.values.end() && itrOldSubscription.key() == itr.key())
        {
            itr.value() = std::max(itr.value(), itrOldSubscription.value());
        }
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
    sendRuntimeData(connection, context(connection)->remoteSubscription.values.keys());
    return true;
}

bool MessageBus::handleSubscribeForAllDataUpdates(
    const P2pConnectionPtr& connection,
    const QByteArray& data)
{
    NX_ASSERT(connection->remotePeer().peerType == PeerType::cloudServer);
    context(connection)->remoteAddImplicitData = true;
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

void MessageBus::gotTransaction(
    const QnTransaction<nx::vms::api::UpdateSequenceData> &tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    PersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);

    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
        printTran(connection, tran, Connection::Direction::incoming);

    updateOfflineDistance(connection, peerId, tran.persistentInfo.sequence);
}

void MessageBus::processRuntimeInfo(
    const QnTransaction<RuntimeData> &tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    if (localPeer().isServer() && !isSubscribedTo(connection->remotePeer()))
        return; // Ignore deprecated transaction.

    PersistentIdData peerId(tran.peerID, tran.params.peer.persistentId);

    if (m_lastRuntimeInfo[peerId] == tran.params)
        return; //< Already processed. Ignore same transaction.

    m_lastRuntimeInfo[peerId] = tran.params;

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
    emitPeerFoundLostSignals();
    // Proxy transaction to subscribed peers
    sendTransaction(tran, transportHeader);
}

template <class T>
void MessageBus::gotTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& transportHeader)
{
    if (processSpecialTransaction(tran, connection, transportHeader))
        return;

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
}

template <class T>
void MessageBus::gotUnicastTransaction(
    const QnTransaction<T>& tran,
    const P2pConnectionPtr& connection,
    const TransportHeader& header)
{
    if (nx::utils::log::isToBeLogged(nx::utils::log::Level::verbose, this))
        printTran(connection, tran, Connection::Direction::incoming);

    std::set<QnUuid> unprocessedPeers;
    for (auto& peer: header.dstPeers)
    {
        if (peer == localPeer().id)
        {
            if (m_handler)
                m_handler->triggerNotification(tran, NotificationSource::Remote);
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
            NX_WARNING(this, lm("Drop unicast transaction because no route found"));
            continue;
        }
        if (auto& dstConnection = m_connections.value(dstPeerId))
            dstByConnection[dstConnection].dstPeers.push_back(dstPeer);
        else
            NX_ASSERT(0, lm("Drop unicast transaction. Can't find connection to route"));
    }
    for (TransportHeader& newHeader: dstByConnection)
        newHeader.via = header.via;
    sendUnicastTransactionImpl(tran, dstByConnection);
}

bool MessageBus::handlePushTransactionList(const P2pConnectionPtr& connection, const QByteArray& data)
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
        if (!handlePushTransactionData(connection, transaction, TransportHeader()))
            return false;
    }
    return true;
}

bool MessageBus::handlePushTransactionData(
    const P2pConnectionPtr& connection,
    const QByteArray& serializedTran,
    const TransportHeader& header)
{
    // Shity workaround for compatibility with server 3.1/3.2 with p2p mode on
    // It could send subscribeForDataUpdates binary message among json data.
    // TODO: we have to remove this checking in 4.0 because it is fixed on server side.
    if (localPeerEx().dataFormat == Qn::JsonFormat && !serializedTran.isEmpty()
        && serializedTran[0] == (quint8)MessageType::subscribeForDataUpdates)
    {
        return true; //< Ignore binary message
    }

    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->localPeer().dataFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, connection, header),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });
}

bool MessageBus::isSubscribedTo(const PersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
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
    QnMutexLocker lock(&m_mutex);
    if (PersistentIdData(localPeer()) == peer)
        return 0;
    return m_peers->distanceTo(peer);
}

QMap<QnUuid, P2pConnectionPtr> MessageBus::connections() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connections;
}

int MessageBus::connectionTries() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connectionTries;
}

QSet<QnUuid> MessageBus::directlyConnectedClientPeers() const
{
    QnMutexLocker lock(&m_mutex);
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
    QnMutexLocker lock(&m_mutex);
    return m_connections.keys().toSet();
}

QnUuid MessageBus::routeToPeerVia(
    const QnUuid& peerId, int* distance, nx::network::SocketAddress* knownPeerAddress) const
{
    QnMutexLocker lock(&m_mutex);
    if (knownPeerAddress)
    {
        *knownPeerAddress = nx::network::SocketAddress();
        for (const auto& peer: m_remoteUrls)
        {
            if (peerId == peer.peerId)
            {
                *knownPeerAddress = nx::network::SocketAddress(peer.url.host(), peer.url.port());
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
    QnMutexLocker lock(&m_mutex);
    if (localPeer().id == peerId)
        return 0;
    return m_peers->distanceTo(peerId);
}

ConnectionContext* MessageBus::context(const P2pConnectionPtr& connection)
{
    return static_cast<ConnectionContext*> (connection->opaqueObject());
}

QVector<QnTransportConnectionInfo> MessageBus::connectionsInfo() const
{
    QVector<QnTransportConnectionInfo> result;
    QnMutexLocker lock(&m_mutex);

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
            info.isStarted = context->isLocalStarted;
            info.subscription = context->localSubscription;
            info.peerType = connection->remotePeer().peerType;

            // Has got peerInfo message from remote server. Open new connection is blocked
            // unless server have opening connection that hasn't got this answer from remote server
            // yet. It caused by optimization to do not open all connections to each other.
            info.gotPeerInfo = !context->remotePeersMessage.isEmpty();

            result.push_back(info);
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
        result.push_back(info);
    }

    for (auto& record: result)
        record.previousState = toString(m_lastConnectionState.value(record.remotePeerId));

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
            lit("Peer %1 has found peer %2")
            .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
            .arg(qnStaticCommon->moduleDisplayName(peer.id)));
        emit peerFound(peer.id, peer.peerType);
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
                lit("Peer %1 has lost peer %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(peer.id)));
            emit peerLost(peer.id, peer.peerType);
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
    QnMutexLocker lock(&m_mutex);
    m_intervals = intervals;
}

MessageBus::DelayIntervals MessageBus::delayIntervals() const
{
    QnMutexLocker lock(&m_mutex);
    return m_intervals;
}

QMap<PersistentIdData, RuntimeData> MessageBus::runtimeInfo() const
{
    QnMutexLocker lock(&m_mutex);
    return m_lastRuntimeInfo;
}

void MessageBus::sendInitialDataToCloud(const P2pConnectionPtr& connection)
{
    NX_ASSERT(0, "Not implemented");
}

bool MessageBus::validateRemotePeerData(const vms::api::PeerDataEx& /*remotePeer*/)
{
    return true;
}

} // namespace p2p
} // namespace nx
