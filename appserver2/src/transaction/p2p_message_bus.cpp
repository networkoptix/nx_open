#include "p2p_message_bus.h"

#include <QtCore/QBuffer>

#include <common/common_module.h>
#include <utils/media/bitStream.h>
#include <utils/media/nalUnits.h>
#include <utils/common/synctime.h>
#include <database/db_manager.h>
#include "ec_connection_notification_manager.h"
#include "transaction_message_bus_priv.h"
#include <nx/utils/random.h>
#include "ubjson_transaction_serializer.h"
#include "json_transaction_serializer.h"
#include <api/global_settings.h>
#include <nx_ec/ec_proto_version.h>
#include <utils/math/math.h>

namespace {

    // todo: move these timeouts to the globalSettings

    // How often send 'peers' message to the peer if something is changed
    // As soon as connection is opened first message is sent immediately
    std::chrono::seconds sendPeersInfoInterval(15);

    // If new connection is recently established/closed, don't sent subscribe request to the peer
    std::chrono::seconds subscribeIntervalLow(3);

    // If new connections always established/closed too long time, send subscribe request anyway
    std::chrono::seconds subscribeIntervalHigh(15);

    int commitIntervalMs = 1000;
    static const int kMaxSelectDataSize = 1024 * 32;


    // How many new connections we could try to open at the same time
    static const int kMaxConnectionsAtOnce = 10;

    static const int kOutConnectionsInterval = 1000 * 5;

    static const int kPeerRecordSize = 6;
    static const int kResolvePeerResponseRecordSize = 16 * 2 + 2; //< two guid + uncompressed PeerNumber per record
} // namespace

namespace ec2 {

// PeerInfo

qint32 P2pMessageBus::AlivePeerInfo::distanceTo(const ApiPersistentIdData& via) const
{
    qint32 result = std::numeric_limits<qint32>::max();
    auto itr = routeTo.find(via);
    return itr != routeTo.end() ? itr.value().distance : kMaxDistance;
}

qint32 P2pMessageBus::RouteToPeerInfo::minDistance(QVector<ApiPersistentIdData>* outViaList) const
{
    if (m_minDistance == kMaxDistance)
    {
        for (auto itr = m_routeVia.cbegin(); itr != m_routeVia.cend(); ++itr)
            m_minDistance = std::min(m_minDistance, itr.value().distance);
    }
    if (outViaList)
    {
        for (auto itr = m_routeVia.cbegin(); itr != m_routeVia.cend(); ++itr)
        {
            if (itr.value().distance == m_minDistance)
                outViaList->push_back(itr.key());
        }
    }
    return m_minDistance;
}

// ---------------------- BidirectionRoutingInfo --------------

P2pMessageBus::BidirectionRoutingInfo::BidirectionRoutingInfo(
    const ApiPersistentIdData& localPeer)
:
    m_localPeer(localPeer)
{
    auto& peerData = alivePeers[localPeer].routeTo[localPeer] = RoutingRecord(0, 0);
    allPeerDistances[localPeer].insert(localPeer, RoutingRecord(0, 0));
}

void P2pMessageBus::BidirectionRoutingInfo::clear()
{
    alivePeers.clear();
    allPeerDistances.clear();
}

void P2pMessageBus::BidirectionRoutingInfo::removePeer(const ApiPersistentIdData& id)
{
    alivePeers.remove(id);
    alivePeers[m_localPeer].routeTo.remove(id);

    allPeerDistances[id].remove(m_localPeer);
    for (auto itr = allPeerDistances.begin(); itr != allPeerDistances.end(); ++itr)
        itr->remove(id);
}

void P2pMessageBus::BidirectionRoutingInfo::addRecord(
    const ApiPersistentIdData& via,
    const ApiPersistentIdData& to,
    const RoutingRecord& record)
{
    alivePeers[via].routeTo[to] = record;
    allPeerDistances[to].insert(via, record);
}

qint32 P2pMessageBus::BidirectionRoutingInfo::distanceTo(const ApiPersistentIdData& peer) const
{
    qint32 result = kMaxDistance;
    for (const auto& alivePeer: alivePeers)
        result = std::min(result, alivePeer.distanceTo(peer));
    return result;
}

void P2pMessageBus::BidirectionRoutingInfo::updateLocalDistance(const ApiPersistentIdData& peer, qint32 sequence)
{
    return;
    if (peer == m_localPeer)
        return;
    const qint32 localOfflineDistance = kMaxDistance - sequence;
    addRecord(
        m_localPeer,
        peer,
        RoutingRecord(localOfflineDistance, qnSyncTime->currentMSecsSinceEpoch()));
}

// ---------------------- P2pMessageBus --------------

P2pMessageBus::P2pMessageBus(
    detail::QnDbManager* db,
    Qn::PeerType peerType,
    QnCommonModule* commonModule,
    QnJsonTransactionSerializer* jsonTranSerializer,
    QnUbjsonTransactionSerializer* ubjsonTranSerializer)

:
    QnTransactionMessageBusBase(db, peerType, commonModule, jsonTranSerializer, ubjsonTranSerializer)
{
    qRegisterMetaType<MessageType>();
    qRegisterMetaType<P2pConnection::State>("P2pConnection::State");
    qRegisterMetaType<P2pConnectionPtr>("P2pConnectionPtr");

    m_thread->setObjectName("P2pMessageBus");
    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &P2pMessageBus::doPeriodicTasks);
    m_timer->start(500);
    m_dbCommitTimer.restart();
}

P2pMessageBus::~P2pMessageBus()
{
    stop();
    delete m_timer;
}

void P2pMessageBus::dropConnections()
{
    QnMutexLocker lock(&m_mutex);
    m_connections.clear();
    m_outgoingConnections.clear();
    m_peers->clear();
}


void P2pMessageBus::printTran(
    const P2pConnectionPtr& connection,
    const QnAbstractTransaction& tran,
    P2pConnection::Direction direction) const
{

    auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
    auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);
    QString msgName;
    QString directionName;
    if (direction == P2pConnection::Direction::outgoing)
    {
        msgName = lm("Send");
        directionName = lm("--->");
    }
    else
    {
        msgName = lm("Got");
        directionName = lm("<---");
    }

    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("%1 tran:\t %2 %3 %4. Command: %5. Created by %6")
        .arg(msgName)
        .arg(localPeerName)
        .arg(directionName)
        .arg(remotePeerName)
        .arg(toString(tran.command))
        .arg(qnStaticCommon->moduleDisplayName(tran.peerID)),
        cl_logDEBUG1);
}

void P2pMessageBus::start()
{
    m_localShortPeerInfo.encode(localPeer(), 0);
    m_peers.reset(new BidirectionRoutingInfo(localPeer()));
    addOfflinePeersFromDb();

    base_type::start();
}

void P2pMessageBus::stop()

{
    {
        QnMutexLocker lock(&m_mutex);
        m_remoteUrls.clear();
    }

    dropConnections();
    base_type::stop();
}

// P2pMessageBus

void P2pMessageBus::addOutgoingConnectionToPeer(const QnUuid& id, const QUrl& url)
{
    QnMutexLocker lock(&m_mutex);
    int pos = nx::utils::random::number((int) 0, (int) m_remoteUrls.size());
    m_remoteUrls.insert(m_remoteUrls.begin() + pos, RemoteConnection(id, url));
}

void P2pMessageBus::removeOutgoingConnectionFromPeer(const QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_remoteUrls.size() - 1; ++i)
    {
        if (m_remoteUrls[i].id == id)
        {
            m_remoteUrls.erase(m_remoteUrls.begin() + i);
            break;
        }
    }

    m_outgoingConnections.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == P2pConnection::Direction::outgoing)
        itr.value()->setState(P2pConnection::State::Error);
}

void P2pMessageBus::gotConnectionFromRemotePeer(P2pConnectionPtr connection)
{
    QnMutexLocker lock(&m_mutex);

    const auto remoteId = connection->remotePeer().id;
    m_peers->removePeer(connection->remotePeer());
    m_connections[remoteId] = connection;
    connectSignals(connection);
    connection->startReading();
}

void P2pMessageBus::connectSignals(const P2pConnectionPtr& connection)
{
    QnMutexLocker lock(&m_mutex);
    connect(connection.data(), &P2pConnection::stateChanged, this, &P2pMessageBus::at_stateChanged);
    connect(connection.data(), &P2pConnection::gotMessage, this, &P2pMessageBus::at_gotMessage);
    connect(connection.data(), &P2pConnection::allDataSent, this, &P2pMessageBus::at_allDataSent);
}

void P2pMessageBus::createOutgoingConnections()
{
    if (m_outConnectionsTimer.isValid() && !m_outConnectionsTimer.hasExpired(kOutConnectionsInterval))
        return;
    m_outConnectionsTimer.restart();

    auto itr = m_remoteUrls.begin();

    //for (auto itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    for (int i = 0; i < m_remoteUrls.size(); ++i)
    {
        if (m_outgoingConnections.size() >= kMaxConnectionsAtOnce)
            return; //< wait a bit

        int pos = m_lastOutgoingIndex % m_remoteUrls.size();
        ++m_lastOutgoingIndex;

        const RemoteConnection& remoteConnection = m_remoteUrls[pos];
        if (!m_connections.contains(remoteConnection.id) && !m_outgoingConnections.contains(remoteConnection.id))
        {
            {
                // This check is redundant (can be ommited). But it reduce network race condition time.
                // So, it reduce frequency of in/out conflict and network traffic a bit.
                if (m_connectionGuardSharedState.contains(remoteConnection.id))
                    continue; //< incoming connection in progress
            }

            ConnectionLockGuard connectionLockGuard(
                commonModule()->moduleGUID(),
                connectionGuardSharedState(),
                remoteConnection.id,
                ConnectionLockGuard::Direction::Outgoing);

            P2pConnectionPtr connection(new P2pConnection(
                commonModule(),
                remoteConnection.id,
                localPeerEx(),
                std::move(connectionLockGuard),
                remoteConnection.url));
            m_outgoingConnections.insert(remoteConnection.id, connection);
            ++m_connectionTries;
            connectSignals(connection);
            connection->startConnection();
        }
    }
}

void P2pMessageBus::addOfflinePeersFromDb()
{
    if (!m_db)
        return;
    const auto localPeer = this->localPeer();

    auto persistentState = m_db->transactionLog()->getTransactionsState().values;
    const qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    for (auto itr = persistentState.cbegin(); itr != persistentState.cend(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == localPeer)
            continue;

        const qint32 sequence = itr.value();
        const qint32 localOfflineDistance = kMaxDistance - sequence;
        RoutingRecord record(localOfflineDistance, currentTimeMs);
        m_peers->addRecord(localPeer, peer, record);
    }
}

QByteArray P2pMessageBus::serializePeersMessage(
    const BidirectionRoutingInfo* peers,
    PeerNumberInfo& shortPeerInfo)
{
    QByteArray result;
    result.resize(qPower2Ceil(
        unsigned(peers->allPeerDistances.size() * kPeerRecordSize + 1), 
        4));
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    try
    {
        writer.putBits(8, (int) MessageType::alivePeers);

        // serialize online peers
        for (auto itr = peers->allPeerDistances.cbegin(); itr != peers->allPeerDistances.cend(); ++itr)
        {
            const auto& peer = itr.value();
            qint32 minDistance = peer.minDistance();
            if (minDistance == kMaxDistance)
                continue;
            const qint16 peerNumber = shortPeerInfo.encode(itr.key());
            serializeCompressPeerNumber(writer, peerNumber);
            const bool isOnline = minDistance < kMaxOnlineDistance;
            writer.putBit(isOnline);
            if (isOnline)
                NALUnit::writeUEGolombCode(writer, minDistance); //< distance
            else
                writer.putBits(32, minDistance); //< distance
        }

        writer.flushBits(true);
        result.truncate(writer.getBytesCount());
        return result;
    }
    catch (...)
    {
        return QByteArray();
    }
}

bool P2pMessageBus::deserializePeersMessage(
    const ApiPersistentIdData& remotePeer,
    int remotePeerDistance,
    const PeerNumberInfo& shortPeerInfo,
    const QByteArray& data,
    const qint64 timeMs,
    BidirectionRoutingInfo* outPeers)
{
    outPeers->removePeer(remotePeer);

    BitStreamReader reader((const quint8*) data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            ApiPersistentIdData peerId = shortPeerInfo.decode(peerNumber);
            bool isOnline = reader.getBit();
            qint32 receivedDistance = 0;
            if (isOnline)
                receivedDistance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            else
                receivedDistance = reader.getBits(32);

            outPeers->addRecord(remotePeer, peerId, RoutingRecord(receivedDistance + remotePeerDistance, timeMs));
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

void P2pMessageBus::printPeersMessage()
{
    QList<QString> records;

    for (auto itr = m_peers->alivePeers.constBegin(); itr != m_peers->alivePeers.constEnd(); ++itr)
    {
        const auto& peer = itr.value();

        qint32 minDistance = kMaxDistance;
        for (const RoutingRecord& rec : peer.routeTo)
            minDistance = std::min(minDistance, rec.distance);
        if (minDistance == kMaxDistance)
            continue;
        qint16 peerNumber = m_localShortPeerInfo.encode(itr.key());

        records << lit("\t\t\t\t\t To:  %1(dbId=%2). Distance: %3")
            .arg(qnStaticCommon->moduleDisplayName(itr.key().id))
            .arg(itr.key().persistentId.toString())
            .arg(minDistance);
    }

    NX_LOG(QnLog::P2P_TRAN_LOG,
        lit("Peer %1 records:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(records.join("\n")),
        cl_logDEBUG1);
}

void P2pMessageBus::printSubscribeMessage(
    const QnUuid& remoteId,
    const QVector<ApiPersistentIdData>& subscribedTo) const
{
    QList<QString> records;

    for (const auto& peer: subscribedTo)
    {
        records << lit("\t\t\t\t\t To:  %1(dbId=%2)")
            .arg(qnStaticCommon->moduleDisplayName(peer.id))
            .arg(peer.persistentId.toString());
    }

    NX_LOG(QnLog::P2P_TRAN_LOG, lit("Subscribe:\t %1 ---> %2:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(remoteId))
        .arg(records.join("\n")),
        cl_logDEBUG1);
}

void P2pMessageBus::sendAlivePeersMessage()
{
    QByteArray data = serializePeersMessage(m_peers.get(), m_localShortPeerInfo);
    int peersIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        sendPeersInfoInterval).count();
    for (const auto& connection : m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected)
            continue;
        if (!connection->miscData().isRemoteStarted)
            continue;

        auto& miscData = connection->miscData();
        if (miscData.localPeersTimer.isValid() && !miscData.localPeersTimer.hasExpired(peersIntervalMs))
            continue;
        if (data != miscData.localPeersMessage)
        {
            miscData.localPeersMessage = data;
            miscData.localPeersTimer.restart();
            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
                printPeersMessage();
            connection->sendMessage(data);
        }
    }
}

QMap<ApiPersistentIdData, P2pConnectionPtr> P2pMessageBus::getCurrentSubscription() const
{
    QMap<ApiPersistentIdData, P2pConnectionPtr> result;
    for (auto itr = m_connections.cbegin(); itr != m_connections.cend(); ++itr)
    {
        for (const auto peer: itr.value()->miscData().localSubscription)
            result[peer] = itr.value();
    }
    return result;
}

void P2pMessageBus::resubscribePeers(
    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription)
{
    QMap<P2pConnectionPtr, QVector<ApiPersistentIdData>> updatedSubscription;
    for (auto itr = newSubscription.begin(); itr != newSubscription.end(); ++itr)
        updatedSubscription[itr.value()].push_back(itr.key());
    for (auto& value: updatedSubscription)
        std::sort(value.begin(), value.end());
    for (auto connection: m_connections.values())
    {
        const auto& newValue = updatedSubscription.value(connection);
        auto& miscData = connection->miscData();
        if (miscData.localSubscription != newValue)
        {
            miscData.localSubscription = newValue;
            QVector<PeerNumberType> shortValues;
            for (const auto& id: newValue)
                shortValues.push_back(connection->encode(id));
            if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
                printSubscribeMessage(connection->remotePeer().id, miscData.localSubscription);

            NX_ASSERT(newValue.contains(connection->remotePeer()));

            connection->sendMessage(serializeSubscribeRequest(
                shortValues,
                m_db->transactionLog()->getTransactionsState(newValue)));
        }
    }
}

P2pConnectionPtr P2pMessageBus::findConnectionById(const ApiPersistentIdData& id) const
{
    P2pConnectionPtr result =  m_connections.value(id.id);
    return result && result->remotePeer().persistentId == id.persistentId ? result : P2pConnectionPtr();
}

#if 0
P2pMessageBus::RouteToPeerMap P2pMessageBus::allPeersDistances() const
{
    if (!m_allPeerDistances.isEmpty())
        return m_allPeerDistances;
    for (auto itrVia = m_alivePeers.constBegin(); itrVia != m_alivePeers.constEnd(); ++itrVia)
    {
        const auto& toPeers = itrVia.value().routeTo;
        for (auto itrTo = toPeers.begin(); itrTo != toPeers.end(); ++itrTo)
        {
            const auto& record = itrTo.value();
            m_allPeerDistances[itrTo.key()].routeVia[itrVia.key()] = record;
        }
    }
    return m_allPeerDistances;
}
#endif

bool P2pMessageBus::needSubscribeDelay()
{
    // If alive peers has been recently received, postpone subscription for some time.
    // This peer could be found via another neighbor.
    if (m_lastPeerInfoTimer.isValid())
    {
        std::chrono::milliseconds lastPeerInfoElapsed(m_lastPeerInfoTimer.elapsed());

        if (lastPeerInfoElapsed < subscribeIntervalLow)
        {
            if (!m_wantToSubscribeTimer.isValid())
                m_wantToSubscribeTimer.restart();
            std::chrono::milliseconds wantToSubscribeElapsed(m_wantToSubscribeTimer.elapsed());
            if (wantToSubscribeElapsed < subscribeIntervalHigh)
                return true;
        }
    }
    m_wantToSubscribeTimer.invalidate();
    return false;
}

void P2pMessageBus::startStopConnections(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;

    for (const auto& connection : m_connections)
    {
        const auto& data = connection->miscData();
        if (data.isLocalStarted && data.remotePeersMessage.isEmpty())
        {
            auto& miscData = connection->miscData();
            return;
        }
    }

    int counter = kMaxConnectionsAtOnce;
    // start using connection if need
    const int kMaxSubscriptionToResubscribe = std::sqrt(std::max(m_connections.size(), (int) m_remoteUrls.size()));
    for (auto& connection: m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected ||  connection->miscData().isLocalStarted)
            continue; //< already in use or not ready yet

        ApiPersistentIdData peer = connection->remotePeer();
        qint32 currentDistance = allPeerDistances.value(peer).minDistance();
        const auto& subscribedVia = currentSubscription.value(peer);
        static const int kMaxDistanceToUseProxy = 2;
        if (currentDistance > kMaxDistanceToUseProxy ||
            (subscribedVia && subscribedVia->miscData().localSubscription.size() > kMaxSubscriptionToResubscribe))
        {
            connection->miscData().isLocalStarted = true;
            connection->miscData().sendStartTimer.restart();
            connection->sendMessage(MessageType::start, QByteArray());
            if (--counter == 0)
                return; //< limit start requests at once
        }
    }
}

P2pConnectionPtr P2pMessageBus::findBestConnectionToSubscribe(
    const QVector<ApiPersistentIdData>& viaList) const
{
    P2pConnectionPtr result;
    for (const auto& via: viaList)
    {
        auto connection = findConnectionById(via);
        NX_ASSERT(connection);
        if (!connection)
            continue;
        if (!result ||
            connection->miscData().localSubscription.size() < result->miscData().localSubscription.size())
        {
            result = connection;
        }
    }

    return result;
}

void P2pMessageBus::doSubscribe(const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription)
{
    const bool needDelay = needSubscribeDelay();

    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription = currentSubscription;
    const auto localPeer = this->localPeer();
    bool isUpdated = false;

    const RouteToPeerMap& allPeerDistances = m_peers->allPeerDistances;
    const auto& alivePeers =  m_peers->alivePeers;

    for (auto itr = allPeerDistances.constBegin(); itr != allPeerDistances.constEnd(); ++itr)
    {
        const ApiPersistentIdData& peer = itr.key();
        if (peer == localPeer)
            continue;

        auto subscribedVia = currentSubscription.value(peer);
        qint32 subscribedDistance =
            alivePeers[subscribedVia ? subscribedVia->remotePeer() : localPeer].distanceTo(peer);

        const auto& info = itr.value();
        qint32 minDistance = info.minDistance();
        if (minDistance < subscribedDistance)
        {
            if (needDelay && minDistance > 1)
                continue; //< allow only direct subscription if network configuration are still changing
            QVector<ApiPersistentIdData> viaList;
            info.minDistance(&viaList);

            NX_ASSERT(!viaList.empty());
            // If any of connections with min distance subscribed to us then postpone our subscription.
            // It could happen if neighbor(or closer) peer just goes offline
            if (std::any_of(viaList.begin(), viaList.end(),
                [this, &peer](const ApiPersistentIdData& via)
            {
                auto connection = findConnectionById(via);
                NX_ASSERT(connection);
                return connection->remotePeerSubscribedTo(peer);
            }))
            {
                continue;
            }

            auto connection = findBestConnectionToSubscribe(viaList);
            if (subscribedVia)
            {
                NX_LOG(QnLog::P2P_TRAN_LOG,
                    lit("Peer %1 is changing subscription to %2. subscribed via %3. new subscribe via %4")
                    .arg(qnStaticCommon->moduleDisplayName(localPeer.id))
                    .arg(qnStaticCommon->moduleDisplayName(peer.id))
                    .arg(qnStaticCommon->moduleDisplayName(subscribedVia->remotePeer().id))
                    .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
                    cl_logDEBUG1);
            }
            newSubscription[peer] = connection;
            isUpdated = true;
        }
    }
    if (isUpdated)
        resubscribePeers(newSubscription);
}

void P2pMessageBus::commitLazyData()
{
    if (m_dbCommitTimer.hasExpired(commitIntervalMs))
    {
        detail::QnDbManager::QnDbTransactionLocker dbTran(m_db->getTransaction());
        dbTran.commit();
        m_dbCommitTimer.restart();
    }
}

void P2pMessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);

    createOutgoingConnections(); //< open new connections

    sendAlivePeersMessage();
    const QMap<ApiPersistentIdData, P2pConnectionPtr>& currentSubscription = getCurrentSubscription();
    startStopConnections(currentSubscription);
    doSubscribe(currentSubscription);
    commitLazyData();
}

ApiPeerData P2pMessageBus::localPeer() const
{
    return ApiPeerData(
        commonModule()->moduleGUID(),
        commonModule()->runningInstanceGUID(),
        commonModule()->dbId(),
        m_localPeerType);
}

ApiPeerDataEx P2pMessageBus::localPeerEx() const
{
    ApiPeerDataEx result;

    result.id = commonModule()->moduleGUID();
    result.persistentId = commonModule()->dbId();
    result.instanceId = commonModule()->runningInstanceGUID();
    result.systemId = commonModule()->globalSettings()->localSystemId();
    result.peerType = Qn::PT_Server;
    result.cloudHost = nx::network::AppInfo::defaultCloudHost();
    result.identityTime = commonModule()->systemIdentityTime();
    result.keepAliveTimeout = commonModule()->globalSettings()->connectionKeepAliveTimeout().count();
    result.protoVersion = nx_ec::EC2_PROTO_VERSION;
    result.dataFormat = Qn::UbjsonFormat;
    return result;
}

void P2pMessageBus::at_stateChanged(
    const QSharedPointer<P2pConnection>& connection,
    P2pConnection::State state)
{
    QnMutexLocker lock(&m_mutex);
    if (!connection)
        return;

    const auto& remoteId = connection->remotePeer().id;
    switch (connection->state())
    {
        case P2pConnection::State::Connected:
            if (connection->direction() == P2pConnection::Direction::outgoing)
            {
                m_connections[remoteId] = connection;
                m_outgoingConnections.remove(remoteId);
                connection->startReading();
            }
            break;
        case P2pConnection::State::Error:
        {
            NX_LOG(QnLog::P2P_TRAN_LOG, lit("Peer %1 has closed connection to %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
                cl_logDEBUG1);


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
            break;
        }
        default:
            break;
    }
}

void P2pMessageBus::at_allDataSent(const QSharedPointer<P2pConnection>& connection)
{
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;
    if (connection->miscData().selectingDataInProgress)
        selectAndSendTransactions(connection, connection->miscData().remoteSubscription);

}

void P2pMessageBus::at_gotMessage(
    const QSharedPointer<P2pConnection>& connection,
    MessageType messageType,
    const nx::Buffer& payload)
{
    if (!connection)
        return;

    QnMutexLocker lock(&m_mutex);
    if (m_connections.value(connection->remotePeer().id) != connection)
        return;


    if (connection->state() == P2pConnection::State::Error)
        return; //< Connection has been closed
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG) &&
        messageType != MessageType::pushTransactionData)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);

        NX_LOG(QnLog::P2P_TRAN_LOG, lit("Got message:\t %1 <--- %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(payload.size() + 1),
            cl_logDEBUG1);
    }


    bool result = false;
    switch (messageType)
    {
    case MessageType::start:
        connection->miscData().isRemoteStarted = true;
        result = true;
        break;
    case MessageType::stop:
        connection->miscData().selectingDataInProgress = false;
        connection->miscData().isRemoteStarted = false;
        connection->miscData().remoteSubscription.values.clear();
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
    case MessageType::pushTransactionData:
        result = handlePushTransactionData(connection, payload);
        break;
    default:
        NX_ASSERT(0, lm("Unknown message type").arg((int)messageType));
        break;
    }
    if (!result)
        connection->setState(P2pConnection::State::Error);
}

QVector<PeerNumberType> P2pMessageBus::deserializeCompressedPeers(const QByteArray& data,  bool* success)
{
    QVector<PeerNumberType> result;
    BitStreamReader reader((const quint8*) data.data(), data.size());
    *success = true;
    try
    {
        while (reader.bitsLeft() >= 8)
            result.push_back(deserializeCompressPeerNumber(reader));
    }
    catch (...)
    {
        *success = false;
    }
    return result;
}

bool P2pMessageBus::handleResolvePeerNumberRequest(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<PeerNumberType> request = deserializeCompressedPeers(data, &success);
    if (!success)
        return false;
    connection->sendMessage(serializeResolvePeerNumberResponse(request));
    return true;
}

void P2pMessageBus::deserializeResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data)
{
    QByteArray response(data);
    QBuffer buffer(&response);
    buffer.open(QIODevice::ReadOnly);
    QDataStream in(&buffer);
    QByteArray tmpBuffer;
    tmpBuffer.resize(16);
    PeerNumberType shortPeerNumber;
    ApiPersistentIdData fullId;
    auto& miscData = connection->miscData();
    while (!in.atEnd())
    {
        in >> shortPeerNumber;
        in.readRawData(tmpBuffer.data(), tmpBuffer.size());
        fullId.id = QnUuid::fromRfc4122(tmpBuffer);
        in.readRawData(tmpBuffer.data(), tmpBuffer.size());
        fullId.persistentId = QnUuid::fromRfc4122(tmpBuffer);
        connection->encode(fullId, shortPeerNumber);

        auto itr = std::find(
            miscData.awaitingNumbersToResolve.begin(),
            miscData.awaitingNumbersToResolve.end(),
            shortPeerNumber);
        if (itr != miscData.awaitingNumbersToResolve.end())
            miscData.awaitingNumbersToResolve.erase(itr);
    }
}

bool P2pMessageBus::handleResolvePeerNumberResponse(const P2pConnectionPtr& connection, const QByteArray& data)
{
    deserializeResolvePeerNumberResponse(connection, data);
    const QByteArray msg = connection->miscData().remotePeersMessage;
    if (!msg.isEmpty())
        handlePeersMessage(connection, msg);
    return true;
}

QByteArray P2pMessageBus::serializeCompressedPeers(MessageType messageType, const QVector<PeerNumberType>& peers)
{
    QByteArray result;
    result.resize(qPower2Ceil(unsigned(peers.size() * 2 + 1), 4));
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    writer.putBits(8, (int)messageType);
    for (const auto& peer : peers)
        serializeCompressPeerNumber(writer, peer);
    writer.flushBits(true);
    result.truncate(writer.getBytesCount());
    return result;
}


QByteArray P2pMessageBus::serializeResolvePeerNumberRequest(const QVector<PeerNumberType>& peers)
{
    return serializeCompressedPeers(MessageType::resolvePeerNumberRequest, peers);
}

QByteArray P2pMessageBus::serializeSubscribeRequest(
    const QVector<PeerNumberType>& peers,
    const QVector<qint32>& sequences)
{
    NX_ASSERT(peers.size() == sequences.size());

    QByteArray result;
    result.resize(qPower2Ceil(unsigned(peers.size() * kPeerRecordSize + 1), 4));
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    writer.putBits(8, (int)MessageType::subscribeForDataUpdates);
    for (int i = 0; i < peers.size(); ++i)
    {
        writer.putBits(16, peers[i]);
        writer.putBits(32, sequences[i]);
    }
    writer.flushBits(true);
    result.truncate(writer.getBytesCount());
    return result;
}

QByteArray P2pMessageBus::serializeResolvePeerNumberResponse(const QVector<PeerNumberType>& peers)
{
    QByteArray result;
    result.reserve(peers.size() * kResolvePeerResponseRecordSize + 1);
    {
        QBuffer buffer(&result);
        buffer.open(QIODevice::WriteOnly);
        QDataStream out(&buffer);

        out << (quint8)(MessageType::resolvePeerNumberResponse);
        for (const auto& peer : peers)
        {
            out << peer;
            ApiPersistentIdData fullId = m_localShortPeerInfo.decode(peer);
            out.writeRawData(fullId.id.toRfc4122().data(), 16);
            out.writeRawData(fullId.persistentId.toRfc4122().data(), 16);
        }
    }
    return result;
}

bool P2pMessageBus::handlePeersMessage(const P2pConnectionPtr& connection, const QByteArray& data)
{

    NX_ASSERT(!data.isEmpty());
    m_lastPeerInfoTimer.restart();
    QVector<PeerNumberType> numbersToResolve;
    BitStreamReader reader((const quint8*)data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            bool isOnline = reader.getBit();
            isOnline
                ? NALUnit::extractUEGolombCode(reader)
                : reader.skipBits(32); //< skip distance
            if (connection->decode(peerNumber).isNull())
                numbersToResolve.push_back(peerNumber);
        }
    }
    catch (...)
    {
        return false; //< invalid message
    }

    connection->miscData().remotePeersMessage = data;
    NX_ASSERT(!data.isEmpty());

    if (numbersToResolve.empty())
    {
        return deserializePeersMessage(
            connection->remotePeer(),
            1, //< remote peer distance
            connection->shortPeers(),
            data,
            qnSyncTime->currentMSecsSinceEpoch(),
            m_peers.get());
    }

    auto& miscData = connection->miscData();
    std::sort(numbersToResolve.begin(), numbersToResolve.end());
    QVector<PeerNumberType> moreNumbersToResolve;

    std::set_difference(
        numbersToResolve.begin(),
        numbersToResolve.end(),
        miscData.awaitingNumbersToResolve.begin(),
        miscData.awaitingNumbersToResolve.end(),
        std::inserter(moreNumbersToResolve, moreNumbersToResolve.begin()));

    if (moreNumbersToResolve.isEmpty())
        return true;

    for (const auto& number: moreNumbersToResolve)
        miscData.awaitingNumbersToResolve.push_back(number);
    std::sort(miscData.awaitingNumbersToResolve.begin(), miscData.awaitingNumbersToResolve.end());
    connection->sendMessage(serializeResolvePeerNumberRequest(moreNumbersToResolve));
    return true;
}

QVector<SubscribeRecord> P2pMessageBus::deserializeSubscribeForDataUpdatesRequest(const QByteArray& data, bool* success)
{
    QVector<SubscribeRecord> result;
    if (data.isEmpty())
        return result;
    BitStreamReader reader((quint8*) data.data(), data.size());
    try {
        while (reader.bitsLeft() > 0)
        {
            PeerNumberType peer = reader.getBits(16);
            qint32 sequence = reader.getBits(32);
            result.push_back(SubscribeRecord(peer, sequence));
        }
        *success = true;
    }
    catch (...)
    {
        *success = false;
    }
    return result;
}

struct SendTransactionToTransportFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        P2pMessageBus* bus,
        const QnTransaction<T> &transaction,
        const P2pConnectionPtr& connection) const
    {
        ApiPersistentIdData tranId(transaction.peerID, transaction.persistentInfo.dbID);
        NX_ASSERT(connection->remotePeerSubscribedTo(tranId));
        NX_ASSERT(!(ApiPersistentIdData(connection->remotePeer()) == tranId));

        if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
            bus->printTran(connection, transaction, P2pConnection::Direction::outgoing);

        switch (connection->remotePeer().dataFormat)
        {
            case Qn::JsonFormat:
                connection->sendMessage(
                    bus->jsonTranSerializer()->serializedTransactionWithoutHeader(transaction) + QByteArray("\r\n"));
                break;
            case Qn::UbjsonFormat:
                connection->sendMessage(MessageType::pushTransactionData,
                    bus->ubjsonTranSerializer()->serializedTransactionWithoutHeader(transaction));
                break;
            default:
                qWarning() << "Client has requested data in an unsupported format" << connection->remotePeer().dataFormat;
                break;
        }
    }
};

struct SendTransactionToTransportFastFuction
{
    bool operator()(
        P2pMessageBus* bus,
        Qn::SerializationFormat /* srcFormat */,
        const QByteArray& serializedTran,
        const P2pConnectionPtr& connection) const
    {
        if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
        {
            QnAbstractTransaction transaction;
            QnUbjsonReader<QByteArray> stream(&serializedTran);
            if (QnUbjson::deserialize(&stream, &transaction))
                bus->printTran(connection, transaction, P2pConnection::Direction::outgoing);
            ApiPersistentIdData tranId(transaction.peerID, transaction.persistentInfo.dbID);
            NX_ASSERT(connection->remotePeerSubscribedTo(tranId));
        }

        connection->sendMessage(MessageType::pushTransactionData, serializedTran);
        return true;
    }
};


bool P2pMessageBus::handleSubscribeForDataUpdates(const P2pConnectionPtr& connection, const QByteArray& data)
{
    bool success = false;
    QVector<SubscribeRecord> request = deserializeSubscribeForDataUpdatesRequest(data, &success);
    if (!success)
        return false;
    QnTranState newSubscription;
    for (const auto& shortPeer : request)
    {
        const auto& id = m_localShortPeerInfo.decode(shortPeer.peer);
        NX_ASSERT(!id.isNull());
        newSubscription.values.insert(id, shortPeer.sequence);
    }

    // merge current and new subscription
    QnTranState& oldSubscription = connection->miscData().remoteSubscription;

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
    NX_ASSERT(!connection->remotePeerSubscribedTo(connection->remotePeer()));
    if (connection->miscData().selectingDataInProgress)
    {
        connection->miscData().remoteSubscription = newSubscription;
        return true;
    }

    return selectAndSendTransactions(connection, std::move(newSubscription));
}

bool P2pMessageBus::selectAndSendTransactions(
    const P2pConnectionPtr& connection,
    QnTranState newSubscription)
{

    QList<QByteArray> serializedTransactions;
    bool isFinished = false;
    const ErrorCode errorCode = m_db->transactionLog()->getExactTransactionsAfter(
        &newSubscription,
        connection->remotePeer().peerType == Qn::PeerType::PT_CloudServer,
        serializedTransactions,
        kMaxSelectDataSize,
        &isFinished);
    connection->miscData().selectingDataInProgress = !isFinished;
    if (errorCode != ErrorCode::ok)
    {
        NX_LOG(
            QnLog::P2P_TRAN_LOG,
            lit("P2P message bus failure. Can't select transaction list from database"),
            cl_logWARNING);
        return false;
    }
    connection->miscData().remoteSubscription = newSubscription;

    using namespace std::placeholders;
    for (const auto& serializedTran: serializedTransactions)
    {
        if (!handleTransaction(
            this,
            connection->remotePeer().dataFormat,
            serializedTran,
            std::bind(
                SendTransactionToTransportFuction(),
                this, _1, connection),
            std::bind(
                SendTransactionToTransportFastFuction(),
                this, _1, _2, connection)))
        {
            connection->setState(P2pConnection::State::Error);
        }
    }
    return true;
}

void P2pMessageBus::proxyFillerTransaction(const QnAbstractTransaction& tran)
{
    // proxy filler transaction to avoid gaps in the persistent sequence
    QnTransaction<ApiUpdateSequenceData> fillerTran(tran);
    fillerTran.command = ApiCommand::updatePersistentSequence;

    auto errCode = m_db->transactionLog()->updateSequence(fillerTran, TranLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ec2::ErrorCode::ok:
            sendTransaction(fillerTran);
            break;
        default:
            resotreAfterDbError();
            break;
    }
}

void P2pMessageBus::gotTransaction(
    const QnTransaction<ApiUpdateSequenceData> &tran,
    const P2pConnectionPtr& connection)
{
    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop

    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
        printTran(connection, tran, P2pConnection::Direction::incoming);

    if (!m_db)
        return;
    auto errCode = m_db->transactionLog()->updateSequence(tran, TranLockType::Lazy);
    switch (errCode)
    {
        case ErrorCode::containsBecauseSequence:
            break;
        case ErrorCode::ok:
            // Proxy transaction to subscribed peers
            m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
            sendTransaction(tran);
            break;
        default:
            connection->setState(P2pConnection::State::Error);
            resotreAfterDbError();
            break;
    }
}

template <class T>
void P2pMessageBus::gotTransaction(
    const QnTransaction<T> &tran,
    const P2pConnectionPtr& connection)
{
    if (nx::utils::log::isToBeLogged(cl_logDEBUG1, QnLog::P2P_TRAN_LOG))
        printTran(connection, tran, P2pConnection::Direction::incoming);

    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
#if 0
    if (!connection->localPeerSubscribedTo(peerId))
    {
        NX_LOG(lit("Peer %1 has not subscribed to %2 via %3. Ignore transaction")
            .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
            .arg(qnStaticCommon->moduleDisplayName(peerId.id))
            .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
            cl_logDEBUG1);
        int gg = 4;
    }
#endif

    //NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop


    if (m_db)
    {
        if (!tran.persistentInfo.isNull())
        {

            std::unique_ptr<detail::QnDbManager::QnLazyTransactionLocker> dbTran;
            dbTran.reset(new detail::QnDbManager::QnLazyTransactionLocker(m_db->getTransaction()));

            QByteArray serializedTran =
                m_ubjsonTranSerializer->serializedTransaction(tran);
            ErrorCode errorCode = dbManager(m_db, connection->getUserAccessData())
                .executeTransactionNoLock(tran, serializedTran);
            switch (errorCode)
            {
            case ErrorCode::ok:
                dbTran->commit();
                m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
                break;
            case ErrorCode::containsBecauseTimestamp:
                dbTran->commit();
                m_peers->updateLocalDistance(peerId, tran.persistentInfo.sequence);
                proxyFillerTransaction(tran);
                return;
            case ErrorCode::containsBecauseSequence:
                dbTran->commit();
                return; // do not proxy if transaction already exists
            default:
                NX_LOG(
                    QnLog::P2P_TRAN_LOG,
                    lit("Can't handle transaction %1: %2. Reopening connection...")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(ec2::toString(errorCode)),
                    cl_logWARNING);
                dbTran.reset(); //< rollback db data
                connection->setState(P2pConnection::State::Error);
                resotreAfterDbError();
                return;
            }
        }

        // Proxy transaction to subscribed peers
        sendTransaction(tran);
    }

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
}

void P2pMessageBus::resotreAfterDbError()
{
    // p2pMessageBus uses lazy transactions. It means it does 1 commit per time.
    // In case of DB error transactionLog state is invalid because part of data was rolled back.
    m_db->transactionLog()->init();
    dropConnections();
}

struct GotTransactionFuction
{
    typedef void result_type;

    template<class T>
    void operator()(
        P2pMessageBus *bus,
        const QnTransaction<T> &transaction,
        const P2pConnectionPtr& connection) const
    {
        bus->gotTransaction(transaction, connection);
    }
};


bool P2pMessageBus::handlePushTransactionData(const P2pConnectionPtr& connection, const QByteArray& serializedTran)
{
    using namespace std::placeholders;
    return handleTransaction(
        this,
        connection->remotePeer().dataFormat,
        std::move(serializedTran),
        std::bind(GotTransactionFuction(), this, _1, connection),
        [](Qn::SerializationFormat, const QByteArray&) { return false; });
}

bool P2pMessageBus::isSubscribedTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return true;
    for (const auto& connection: m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected)
            continue;
        if (connection->localPeerSubscribedTo(peer))
            return true;
    }
    return false;
}

qint32 P2pMessageBus::distanceTo(const ApiPersistentIdData& peer) const
{
    QnMutexLocker lock(&m_mutex);
    if (ApiPersistentIdData(localPeer()) == peer)
        return 0;
    return m_peers->distanceTo(peer);
}

QMap<QnUuid, P2pConnectionPtr> P2pMessageBus::connections() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connections;
}

int P2pMessageBus::connectionTries() const
{
    QnMutexLocker lock(&m_mutex);
    return m_connectionTries;
}

} // ec2
