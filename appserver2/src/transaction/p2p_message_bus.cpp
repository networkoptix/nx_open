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

namespace {

    // todo: move these timeouts to the globalSettings

    // How often send 'peers' message to the peer if something is changed
    // As soon as connection is opened first message is sent immediately
    std::chrono::seconds sendPeersInfoInterval(5);

    // If new connection is recently established/closed, don't sent subscribe request to the peer
    std::chrono::seconds subscribeIntervalLow(3);

    // If new connections always established/closed too long time, send subscribe request anyway
    std::chrono::seconds subscribeIntervalHigh(10);
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
    qint32 minDistance = kMaxDistance;
    for (auto itr = routeVia.begin(); itr != routeVia.end(); ++itr)
        minDistance = std::min(minDistance, itr.value().distance);
    if (outViaList)
    {
        for (auto itr = routeVia.begin(); itr != routeVia.end(); ++itr)
        {
            if (itr.value().distance == minDistance)
                outViaList->push_back(itr.key());
        }
    }
    return minDistance;
}

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
    m_lastSubscribeTimer.restart();
    m_timer->start(500);
}

P2pMessageBus::~P2pMessageBus()
{
    stop();
    delete m_timer;
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

    NX_LOG(lit("%1 tran:\t %2 %3 %4. Command: %5. Created by %6")
        .arg(msgName)
        .arg(localPeerName)
        .arg(directionName)
        .arg(remotePeerName)
        .arg(toString(tran.command))
        .arg(qnStaticCommon->moduleDisplayName(tran.peerID)),
        cl_logDEBUG1);
}

void P2pMessageBus::stop()
{
    base_type::stop();
    m_connections.clear();
}

// P2pMessageBus

void P2pMessageBus::addOutgoingConnectionToPeer(QnUuid& id, const QUrl& url)
{
    QnMutexLocker lock(&m_mutex);
    m_remoteUrls.insert(id, url);
}

void P2pMessageBus::removeOutgoingConnectionFromPeer(QnUuid& id)
{
    QnMutexLocker lock(&m_mutex);

    m_remoteUrls.remove(id);
    m_outgoingConnections.remove(id);
    auto itr = m_connections.find(id);
    if (itr != m_connections.end() && itr.value()->direction() == P2pConnection::Direction::outgoing)
        itr.value()->setState(P2pConnection::State::Error);
}

void P2pMessageBus::gotConnectionFromRemotePeer(P2pConnectionPtr connection)
{
    QnMutexLocker lock(&m_mutex);

    const auto remoteId = connection->remotePeer().id;
    const auto localId = commonModule()->moduleGUID();
    auto itr = m_connections.find(remoteId);

    if (itr == m_connections.end())
    {
        // Got incoming connection. No outgoing connection was done yet
        m_connections.insert(remoteId, connection);
    }
    else
    {
        const auto& existConnection = itr.value();
        if (existConnection->direction() == P2pConnection::Direction::incoming)
        {
            // Replace incoming connection
            itr.value() = connection;
        }
        else if (remoteId > localId)
        {
            // Outgoing connection is present, but incoming connection has bigger priority. Replace it.
            itr.value() = connection;
        }
        else if (existConnection->state() == P2pConnection::State::Connecting)
        {
            // Incoming connection has lower priority, but outgoing connection is not done yet
            // If outgoing connection will success it'll replace incoming connection later
            m_outgoingConnections.insert(remoteId, itr.value());
            itr.value() = connection; //< start using incoming connection unless outgoing will OK.
        }
        else if (existConnection->state() == P2pConnection::State::Error)
        {
            itr.value() = connection;
        }
        else
        {
            // ignore incoming connection because we have active outgoing connection with bigger priority
            return;
        }
    }
    connect(connection.data(), &P2pConnection::gotMessage, this, &P2pMessageBus::at_gotMessage);
    connect(connection.data(), &P2pConnection::stateChanged, this, &P2pMessageBus::at_stateChanged);
    addRouteToDirectlyConnectedPeer(connection);
}

void P2pMessageBus::addRouteToDirectlyConnectedPeer(const P2pConnectionPtr& connection)
{
#if 0
    // add records which are connected local
    if (connection->remotePeer().persistentId.isNull())
    {
        int gg = 4;
    }
    RoutingRecord& record = m_alivePeers[localPeer()].routeTo[connection->remotePeer()];
    record.distance = 1;
    record.lastRecvTime = qnSyncTime->currentMSecsSinceEpoch();

    NX_LOG(lit("Local peer %1. Add distance 1 to remote peer %2.")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
        cl_logDEBUG1);
#endif
}

// Temporary outgoing connection list is used to resolve two-connections conflict in case of
// we did outgoing connection and get incoming connection at the same time.
void P2pMessageBus::processTemporaryOutgoingConnections()
{
    for (auto itr = m_outgoingConnections.begin(); itr != m_outgoingConnections.end();)
    {
        auto connection = itr.value();
        if (connection->state() == P2pConnection::State::Connected)
        {
            // move connection to main list
            m_connections[connection->remotePeer().id] = connection;
            itr = m_outgoingConnections.erase(itr);
        }
        else if (connection->state() == P2pConnection::State::Error)
        {
            // Can't connect. Remove it.
            itr = m_outgoingConnections.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void P2pMessageBus::cleanupRoutingRecords(const ApiPersistentIdData& id)
{
    m_alivePeers.remove(id);
    m_alivePeers[localPeer()].routeTo.remove(id);
}

void P2pMessageBus::removeClosedConnections()
{
    for (auto itr = m_connections.begin(); itr != m_connections.end();)
    {
        auto& connection = itr.value();
        if (connection->state() == P2pConnection::State::Error)
        {
            cleanupRoutingRecords(connection->remotePeer());

            NX_LOG(lit("Peer %1 has closed connection to %2")
                .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
                .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
                cl_logDEBUG1);

            itr = m_connections.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void P2pMessageBus::createOutgoingConnections()
{
    for (auto itr = m_remoteUrls.begin(); itr != m_remoteUrls.end(); ++itr)
    {
        const QnUuid remoteId = itr.key();
        const QUrl url = itr.value();
        if (!m_connections.contains(remoteId) && !m_outgoingConnections.contains(remoteId))
        {
            P2pConnectionPtr connection(
                new P2pConnection(commonModule(), remoteId, localPeer(), url));
            connect(connection.data(), &P2pConnection::gotMessage, this, &P2pMessageBus::at_gotMessage);
            connect(connection.data(), &P2pConnection::stateChanged, this, &P2pMessageBus::at_stateChanged);
            m_connections.insert(remoteId, connection);
        }
    }
}

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber)
{
    const static std::array<int, 3> bitsGroups = { 7, 3, 4 };
    const static int mask = 0x3fff;  //< only low 14 bits are supported
    NX_ASSERT(peerNumber <= mask);
    peerNumber &= mask;
    for (const auto& bits: bitsGroups)
    {
        writer.putBits(bits, peerNumber);
        peerNumber >>= bits;
        if (peerNumber == 0)
        {
            writer.putBit(0); //< end of number
            break;
        }
        writer.putBit(1); //< number continuation
    }
}

PeerNumberType deserializeCompressPeerNumber(BitStreamReader& reader)
{
    const static std::array<int, 3> bitsGroups = { 7, 3, 4 };
    PeerNumberType peerNumber = 0;
    for (int i = 0; i < bitsGroups.size(); ++i)
    {
        peerNumber <<= bitsGroups[i];
        peerNumber += reader.getBits(bitsGroups[i]);
        if (i == bitsGroups.size() - 1 || reader.getBit() == 0)
            break;
    }
    return peerNumber;
}

void P2pMessageBus::addOwnfInfoToPeerList()
{
    if (m_alivePeers.isEmpty())
    {
        auto& peerData = m_alivePeers[localPeer()];
        peerData.routeTo.insert(localPeer(), RoutingRecord(0, 0));
    }
}

void P2pMessageBus::addOfflinePeersFromDb()
{
    if (!m_db)
        return;
    const auto localPeer = this->localPeer();
    auto& LocalPeerData = m_alivePeers[localPeer];

    auto persistentState = m_db->transactionLog()->getTransactionsState().values;
    for (auto itr = persistentState.begin(); itr != persistentState.end(); ++itr)
    {
        const auto& peer = itr.key();
        if (peer == localPeer)
            continue;

        const qint32 sequence = itr.value();
        const qint32 localOfflineDistance = kMaxDistance - sequence;
        RoutingRecord record(localOfflineDistance, qnSyncTime->currentMSecsSinceEpoch());
        LocalPeerData.routeTo[peer] = record;
    }

}

QByteArray P2pMessageBus::serializePeersMessage()
{
    QByteArray result;
    result.resize(m_alivePeers.size() * 6 + 1);
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    try
    {
        writer.putBits(8, (int) MessageType::alivePeers);

        // serialize online peers
        for (auto itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
        {
            const auto& peer = itr.value();

            qint32 minDistance = kMaxDistance;
            for (const RoutingRecord& rec : peer.routeTo)
                minDistance = std::min(minDistance, rec.distance);
            if (minDistance == kMaxDistance)
                continue;
            qint16 peerNumber = m_localShortPeerInfo.encode(itr.key());
            serializeCompressPeerNumber(writer, peerNumber);
            bool isOnline = minDistance < kMaxOnlineDistance;
            writer.putBit(isOnline);
            if (isOnline)
                NALUnit::writeUEGolombCode(writer, minDistance); //< distance
            else
                writer.putBits(32, minDistance); //< distance
        }

        writer.flushBits();
        result.truncate(writer.getBytesCount());
        return result;
    }
    catch (...)
    {
        return QByteArray();
    }
}

void P2pMessageBus::printPeersMessage()
{
    QList<QString> records;

    for (auto itr = m_alivePeers.begin(); itr != m_alivePeers.end(); ++itr)
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

    NX_LOG(lit("Peer %1 records:\n%3")
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

    NX_LOG(lit("Subscribe:\t %1 ---> %2:\n%3")
        .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
        .arg(qnStaticCommon->moduleDisplayName(remoteId))
        .arg(records.join("\n")),
        cl_logDEBUG1);
}

void P2pMessageBus::processAlivePeersMessage(
    const P2pConnectionPtr& connection,
    const QByteArray& data)
{
    const auto& remotePeer = connection->remotePeer();
    const auto& localPeer = this->localPeer();
    NX_ASSERT(!(remotePeer == localPeer));

    AlivePeerInfo& peerData = m_alivePeers[remotePeer];
    peerData.routeTo.clear(); //< remove old data

    qint64 timeMs = qnSyncTime->currentMSecsSinceEpoch();
    BitStreamReader reader((const quint8*) data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            ApiPersistentIdData peerId = connection->decode(peerNumber);
            bool isOnline = reader.getBit();
            qint32 receivedDistance = 0;
            if (isOnline)
                receivedDistance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            else
                receivedDistance = reader.getBits(32);

            const qint32 distance = receivedDistance + 1;

            peerData.routeTo.insert(peerId, RoutingRecord(distance, timeMs));
        }
    }
    catch (...)
    {
        connection->setState(P2pConnection::State::Error);
    }
}

void P2pMessageBus::sendAlivePeersMessage()
{
    QByteArray data = serializePeersMessage();
    int peersIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        sendPeersInfoInterval).count();
    for (const auto& connection : m_connections)
    {
        if (connection->state() != P2pConnection::State::Connected)
            continue;
        auto& miscData = connection->miscData();
        if (miscData.localPeersTimer.isValid() && !miscData.localPeersTimer.hasExpired(peersIntervalMs))
            continue;
        if (data != miscData.localPeersMessage)
        {
            miscData.localPeersMessage = data;
            miscData.localPeersTimer.restart();
            if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
                printPeersMessage();
            connection->sendMessage(data);
        }
    }
}

QMap<ApiPersistentIdData, P2pConnectionPtr> P2pMessageBus::getCurrentSubscription() const
{
    QMap<ApiPersistentIdData, P2pConnectionPtr> result;
    for (auto itr = m_connections.begin(); itr != m_connections.end(); ++itr)
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
            if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
                printSubscribeMessage(connection->remotePeer().id, miscData.localSubscription);
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
void P2pMessageBus::doSubscribe()
{
    // If alive peers has been recently received, postpone subscription for some time.
    // This peer could be found via another neighbor.
    if (m_lastPeerInfoTimer.isValid())
    {
        std::chrono::milliseconds lastPeerInfoElapsed(m_lastPeerInfoTimer.elapsed());
        std::chrono::milliseconds subscribeElapsed(m_lastSubscribeTimer.elapsed());
        if (lastPeerInfoElapsed < subscribeIntervalLow &&
            subscribeElapsed < subscribeIntervalHigh)
        {
            return;
        }
    }
    m_lastSubscribeTimer.restart();


    QMap<ApiPersistentIdData, P2pConnectionPtr> currentSubscription = getCurrentSubscription();
    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription = currentSubscription;
    const auto localPeer = this->localPeer();
    bool isUpdated = false;

    for (auto itr = m_alivePeers.constBegin(); itr != m_alivePeers.constEnd(); ++itr)
    {
        const ApiPersistentIdData& peer = itr.key();
        if (peer == localPeer)
            continue;
        const PeerInfo& info = itr.value();
        auto subscribedVia = currentSubscription.value(peer);
        qint32 subscribedDistance = info.distanceVia(
            subscribedVia ? subscribedVia->remotePeer() : localPeer);

        QVector<ApiPersistentIdData> viaList;
        qint32 minDistance = info.minDistance(&viaList);
        if (minDistance < subscribedDistance)
        {
            NX_ASSERT(!viaList.empty());
            auto itr = std::find(viaList.begin(), viaList.end(), localPeer);
            if (itr != viaList.end())
            {
                // minDistance is in local data. It could happened if peer just goes to offline.
                // Not need to resubscribe.
                //continue;
                int gg = 4;
            }
            // in case of several equal routes, use any(random) of them
            int rndValue = nx::utils::random::number(0, (int) viaList.size() - 1);
            auto connection = findConnectionById(viaList[rndValue]);
            if (subscribedVia)
            {
                NX_LOG(lit("Peer %1 is changing subscription to %2. subscribed via %3. new subscribe via %4")
                    .arg(qnStaticCommon->moduleDisplayName(localPeer.id))
                    .arg(qnStaticCommon->moduleDisplayName(peer.id))
                    .arg(qnStaticCommon->moduleDisplayName(subscribedVia->remotePeer().id))
                    .arg(qnStaticCommon->moduleDisplayName(viaList[rndValue].id)),
                    cl_logDEBUG1);
            }
            if (!connection)
            {
                int gg = 4;
            }
            NX_ASSERT(connection);
            newSubscription[peer] = connection;
            isUpdated = true;
        }
    }
    if (isUpdated)
        resubscribePeers(newSubscription);
}
#endif

P2pMessageBus::RouteToPeerMap P2pMessageBus::allPeersDistances() const
{
    P2pMessageBus::RouteToPeerMap result;
    for (auto itrVia = m_alivePeers.begin(); itrVia != m_alivePeers.end(); ++itrVia)
    {
        const auto& toPeers = itrVia.value().routeTo;
        for (auto itrTo = toPeers.begin(); itrTo != toPeers.end(); ++itrTo)
        {
            const auto& record = itrTo.value();
            result[itrTo.key()].routeVia[itrVia.key()] = record;
        }
    }
    return result;
}

void P2pMessageBus::doSubscribe()
{
    // If alive peers has been recently received, postpone subscription for some time.
    // This peer could be found via another neighbor.
    if (m_lastPeerInfoTimer.isValid())
    {
        std::chrono::milliseconds lastPeerInfoElapsed(m_lastPeerInfoTimer.elapsed());
        std::chrono::milliseconds subscribeElapsed(m_lastSubscribeTimer.elapsed());
        if (lastPeerInfoElapsed < subscribeIntervalLow &&
            subscribeElapsed < subscribeIntervalHigh)
        {
            return;
        }
    }
    m_lastSubscribeTimer.restart();


    QMap<ApiPersistentIdData, P2pConnectionPtr> currentSubscription = getCurrentSubscription();
    QMap<ApiPersistentIdData, P2pConnectionPtr> newSubscription = currentSubscription;
    const auto localPeer = this->localPeer();
    bool isUpdated = false;

    RouteToPeerMap allPeerDistances = allPeersDistances();
    for (auto itr = allPeerDistances.constBegin(); itr != allPeerDistances.constEnd(); ++itr)
    {
        const ApiPersistentIdData& peer = itr.key();
        if (peer == localPeer)
            continue;

        auto subscribedVia = currentSubscription.value(peer);
        qint32 subscribedDistance =
            m_alivePeers[subscribedVia ? subscribedVia->remotePeer() : localPeer].distanceTo(peer);

        QVector<ApiPersistentIdData> viaList;
        const auto& info = itr.value();
        qint32 minDistance = info.minDistance(&viaList);
        if (minDistance < subscribedDistance)
        {
            NX_ASSERT(!viaList.empty());
            auto itr = std::find(viaList.begin(), viaList.end(), localPeer);
            if (itr != viaList.end())
            {
                // minDistance is in local data. It could happened if peer just goes to offline.
                // Not need to resubscribe.
                //continue;
                int gg = 4;
            }
            // in case of several equal routes, use any(random) of them
            int rndValue = nx::utils::random::number(0, (int)viaList.size() - 1);
            auto connection = findConnectionById(viaList[rndValue]);
            if (subscribedVia)
            {
                NX_LOG(lit("Peer %1 is changing subscription to %2. subscribed via %3. new subscribe via %4")
                    .arg(qnStaticCommon->moduleDisplayName(localPeer.id))
                    .arg(qnStaticCommon->moduleDisplayName(peer.id))
                    .arg(qnStaticCommon->moduleDisplayName(subscribedVia->remotePeer().id))
                    .arg(qnStaticCommon->moduleDisplayName(viaList[rndValue].id)),
                    cl_logDEBUG1);
            }
            if (!connection)
            {
                int gg = 4;
            }
            NX_ASSERT(connection);
            newSubscription[peer] = connection;
            isUpdated = true;
        }
    }
    if (isUpdated)
        resubscribePeers(newSubscription);
}

void P2pMessageBus::doPeriodicTasks()
{
    QnMutexLocker lock(&m_mutex);

    processTemporaryOutgoingConnections(); //< resolve doubled ingoing + outgoing connections
    removeClosedConnections(); //< remove closed connections
    createOutgoingConnections(); //< open new connections

    addOwnfInfoToPeerList();
    addOfflinePeersFromDb();
    sendAlivePeersMessage();

    doSubscribe();
}

ApiPeerData P2pMessageBus::localPeer() const
{
    return ApiPeerData(
        commonModule()->moduleGUID(),
        commonModule()->runningInstanceGUID(),
        commonModule()->dbId(),
        m_localPeerType);
}

void P2pMessageBus::at_stateChanged(
    const QSharedPointer<P2pConnection>& connection,
    P2pConnection::State state)
{
    if (state == P2pConnection::State::Connected)
        addRouteToDirectlyConnectedPeer(connection);
}

void P2pMessageBus::at_gotMessage(
    const QSharedPointer<P2pConnection>& connection,
    MessageType messageType,
    const nx::Buffer& payload)
{
    QnMutexLocker lock(&m_mutex);

    if (QnLog::instance()->logLevel() >= cl_logDEBUG1 &&
        messageType != MessageType::pushTransactionData)
    {
        auto localPeerName = qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID());
        auto remotePeerName = qnStaticCommon->moduleDisplayName(connection->remotePeer().id);

        NX_LOG(lit("Got message:\t %1 <--- %2. Type: %3. Size=%4")
            .arg(localPeerName)
            .arg(remotePeerName)
            .arg(toString(messageType))
            .arg(payload.size() + 1),
            cl_logDEBUG1);
    }


    bool result = false;
    switch (messageType)
    {
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

    while (!in.atEnd())
    {
        in >> shortPeerNumber;
        in.readRawData(tmpBuffer.data(), tmpBuffer.size());
        fullId.id = QnUuid::fromRfc4122(tmpBuffer);
        in.readRawData(tmpBuffer.data(), tmpBuffer.size());
        fullId.persistentId = QnUuid::fromRfc4122(tmpBuffer);
        connection->encode(fullId, shortPeerNumber);
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
    result.resize(peers.size() * 2 + 1);
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    writer.putBits(8, (int)messageType);
    for (const auto& peer : peers)
        serializeCompressPeerNumber(writer, peer);
    writer.flushBits();
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
    result.resize(peers.size() * 6 + 1);
    BitStreamWriter writer;
    writer.setBuffer((quint8*)result.data(), result.size());
    writer.putBits(8, (int)MessageType::subscribeForDataUpdates);
    for (int i = 0; i < peers.size(); ++i)
    {
        writer.putBits(16, peers[i]);
        writer.putBits(32, sequences[i]);
    }
    writer.flushBits();
    result.truncate(writer.getBytesCount());
    return result;
}

QByteArray P2pMessageBus::serializeResolvePeerNumberResponse(const QVector<PeerNumberType>& peers)
{
    QByteArray result;
    result.reserve(peers.size() * (16 * 2 + 2) + 1); // two guid + uncompressed PeerNumber per record
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
    m_lastPeerInfoTimer.restart();
    QVector<PeerNumberType> numbersToResolve;
    BitStreamReader reader((const quint8*)data.data(), data.size());
    try
    {
        while (reader.bitsLeft() >= 8)
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            bool isOnline = reader.getBit();
            qint32 distance = isOnline
                ? NALUnit::extractUEGolombCode(reader)
                : reader.getBits(32);
            if (connection->decode(peerNumber).isNull())
                numbersToResolve.push_back(peerNumber);
        }
    }
    catch (...)
    {
        return false; //< invalid message
    }
    if (numbersToResolve.empty())
    {
        processAlivePeersMessage(connection, data);
        return true;
    }
    connection->sendMessage(serializeResolvePeerNumberRequest(numbersToResolve));
    connection->miscData().remotePeersMessage = data;
    return true;
}

QVector<SubscribeRecord> P2pMessageBus::deserializeSubscribeForDataUpdatesRequest(const QByteArray& data, bool* success)
{
    QVector<SubscribeRecord> result;
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

        if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
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
        if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
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
    QnTranState tranState;
    auto& selectRequest = tranState.values;
    for (const auto& shortPeer : request)
    {
        const auto& id = m_localShortPeerInfo.decode(shortPeer.peer);
        NX_ASSERT(!id.isNull());
        selectRequest.insert(id, shortPeer.sequence);
    }
    auto newSubscription = selectRequest.keys();
    auto& oldSubscription = connection->miscData().remoteSubscription;

    // remove already subscribed records
    auto itrFilter = oldSubscription.begin();
    for (auto itr = selectRequest.begin(); itr != selectRequest.end();)
    {
        while (itrFilter != oldSubscription.end() && *itrFilter < itr.key())
            ++itrFilter;
        if (itrFilter != oldSubscription.end() && *itrFilter == itr.key())
            itr = selectRequest.erase(itr); //< filter record
        else
            ++itr;
    }
    connection->miscData().remoteSubscription = newSubscription.toVector();
    NX_ASSERT(!connection->remotePeerSubscribedTo(connection->remotePeer()));

    QList<QByteArray> serializedTransactions;
    const ErrorCode errorCode = m_db->transactionLog()->getExactTransactionsAfter(
        tranState,
        connection->remotePeer().peerType == Qn::PeerType::PT_CloudServer,
        serializedTransactions);
    if (errorCode != ErrorCode::ok)
    {
        NX_LOG(
            QnLog::EC2_TRAN_LOG,
            lit("P2P message bus failure. Can't select transaction list from database"),
            cl_logWARNING);
        return false;
    }

    using namespace std::placeholders;
    for (const auto& serializedTran : serializedTransactions)
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
    m_db->transactionLog()->updateSequence(fillerTran);
    sendTransaction(fillerTran);
}

void P2pMessageBus::gotTransaction(
    const QnTransaction<ApiUpdateSequenceData> &tran,
    const P2pConnectionPtr& connection)
{
    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop

    if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
        printTran(connection, tran, P2pConnection::Direction::incoming);

    if (!m_db)
        return;
    m_db->transactionLog()->updateSequence(tran);
    // Proxy transaction to subscribed peers

    sendTransaction(tran);
}

template <class T>
void P2pMessageBus::gotTransaction(
    const QnTransaction<T> &tran,
    const P2pConnectionPtr& connection)
{
    if (QnLog::instance()->logLevel() >= cl_logDEBUG1)
        printTran(connection, tran, P2pConnection::Direction::incoming);

    ApiPersistentIdData peerId(tran.peerID, tran.persistentInfo.dbID);
    if (!connection->localPeerSubscribedTo(peerId))
    {
        NX_LOG(lit("Peer %1 has not subscribed to %2 via %3. Ignore transaction")
            .arg(qnStaticCommon->moduleDisplayName(localPeer().id))
            .arg(qnStaticCommon->moduleDisplayName(peerId.id))
            .arg(qnStaticCommon->moduleDisplayName(connection->remotePeer().id)),
            cl_logDEBUG1);
        int gg = 4;
    }

    NX_ASSERT(connection->localPeerSubscribedTo(peerId)); //< loop
    NX_ASSERT(!connection->remotePeerSubscribedTo(peerId)); //< loop


    if (m_db)
    {
        if (!tran.persistentInfo.isNull())
        {
            QByteArray serializedTran =
                m_ubjsonTranSerializer->serializedTransaction(tran);
            ErrorCode errorCode = dbManager(m_db, connection->getUserAccessData())
                .executeTransaction(tran, serializedTran);
            switch (errorCode)
            {
            case ErrorCode::ok:
                break;
            case ErrorCode::containsBecauseTimestamp:
                proxyFillerTransaction(tran);
            case ErrorCode::containsBecauseSequence:
                return; // do not proxy if transaction already exists
            default:
                NX_LOG(
                    QnLog::EC2_TRAN_LOG,
                    lit("Can't handle transaction %1: %2. Reopening connection...")
                    .arg(ApiCommand::toString(tran.command))
                    .arg(ec2::toString(errorCode)),
                    cl_logWARNING);
                connection->setState(P2pConnection::State::Error);
                return;
            }
        }

        // Proxy transaction to subscribed peers
        sendTransaction(tran);
    }

    if (m_handler)
        m_handler->triggerNotification(tran, NotificationSource::Remote);
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


} // ec2
