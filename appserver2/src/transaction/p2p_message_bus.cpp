#include "p2p_message_bus.h"
#include <common/common_module.h>
#include <utils/media/bitStream.h>
#include <utils/media/nalUnits.h>
#include <utils/common/synctime.h>
#include <database/db_manager.h>

namespace ec2 {

// PeerNumberInfo

PeerNumberType P2pMessageBus::PeerNumberInfo::insert(const ApiPersistentIdData& peer)
{
    PeerNumberType result = fullIdToShortId.size();
    fullIdToShortId.insert(peer, result);
    shortIdToFullId.insert(result, peer);
    return result;
}

// PeerInfo

quint32 P2pMessageBus::PeerInfo::distanceVia(const ApiPersistentIdData& via) const
{
    quint32 result = std::numeric_limits<quint32>::max();
    auto itr = routingInfo.find(via);
    return itr != routingInfo.end() ? itr.value().distance : kMaxDistance;
}

quint32 P2pMessageBus::PeerInfo::minDistance(std::vector<ApiPersistentIdData>* outViaList) const
{
    quint32 minDistance = kMaxDistance;
    for (auto itr = routingInfo.begin(); itr != routingInfo.end(); ++itr)
        minDistance = std::min(minDistance, itr.value().distance);
    if (outViaList)
    {
        for (auto itr = routingInfo.begin(); itr != routingInfo.end(); ++itr)
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
    QnCommonModule* commonModule)
:
    QnTransactionMessageBusBase(db, peerType, commonModule)
{
    m_thread = new QThread();
    m_thread->setObjectName("P2pMessageBus");
    moveToThread(m_thread);
    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &P2pMessageBus::doPeriodicTasks);
    m_timer->start(500);
}

P2pMessageBus::~P2pMessageBus()
{
    if (m_thread)
    {
        m_thread->exit();
        m_thread->wait();
    }

    delete m_thread;
    delete m_timer;
}

void P2pMessageBus::start()
{
    NX_ASSERT(!m_thread->isRunning());
    if (!m_thread->isRunning())
        m_thread->start();
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
        m_connections.erase(itr);
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
        return;
    }

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
    }
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

void P2pMessageBus::removeClosedConnections()
{
    for (auto itr = m_connections.begin(); itr != m_connections.end();)
    {
        auto& connection = itr.value();
        if (connection->state() == P2pConnection::State::Error)
            itr = m_connections.erase(itr);
        else
            ++itr;
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
            m_connections.insert(remoteId, P2pConnectionPtr(
                new P2pConnection(commonModule(), remoteId, localPeer(), url)));
        }
    }
}

PeerNumberType P2pMessageBus::toShortPeerNumber(const QnUuid& owner, const ApiPersistentIdData& peer)
{
    PeerNumberInfo& info = m_shortPeersMap[owner];
    auto itr = info.fullIdToShortId.find(peer);
    if (itr != info.fullIdToShortId.end())
        return itr.value();
    if (owner == commonModule()->moduleGUID())
        return info.insert(peer);
    return kUnknownPeerNumnber;
}

ApiPersistentIdData P2pMessageBus::fromShortPeerNumber(const QnUuid& owner, const PeerNumberType& id)
{
    return ApiPersistentIdData(); // implement me
}

void serializeCompressPeerNumber(BitStreamWriter& writer, PeerNumberType peerNumber)
{
    const static std::array<int, 3> bitsGroups = { 7, 3, 4 };
    const static int mask = 0x3fff;  //< only low 14 bits are supported
    NX_ASSERT(peerNumber <= mask);
    peerNumber &= mask;
    for (const auto& bits: bitsGroups)
    {
        writer.putBits(peerNumber, bits);
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
    if (m_allPeers.isEmpty())
    {
        auto& peerData = m_allPeers[localPeer()];
        peerData.isOnline = true;
        peerData.routingInfo.insert(localPeer(), RoutingRecord(0, 0));
    }
}

void P2pMessageBus::addOfflinePeersFromDb()
{
    if (!m_db)
        return;

    auto persistentState = m_db->transactionLog()->getTransactionsState().values;
    for (auto itr = persistentState.begin(); itr != persistentState.end(); ++itr)
    {
        ApiPersistentIdData peer(
            itr.key().peerID,
            itr.key().dbID);  //< persistent id
        auto& peerData = m_allPeers[peer];
        if (!peerData.isOnline)
        {
            quint32 sequence = itr.value();
            peerData.routingInfo.insert(localPeer(), RoutingRecord(sequence, qnSyncTime->currentMSecsSinceEpoch()));
        }
    }
}

QByteArray P2pMessageBus::serializePeersMessage()
{
    QByteArray result;
    result.resize(m_allPeers.size() * 6 + 1);
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    try
    {
        writer.putBits(8, (int) MessageType::alivePeers);

        // serialize online peers
        for (auto itr = m_allPeers.begin(); itr != m_allPeers.end(); ++itr)
        {
            const auto& peer = itr.value();

            quint32 minDistance = kMaxDistance;
            for (const RoutingRecord& rec : peer.routingInfo)
                minDistance = std::min(minDistance, rec.distance);
            if (minDistance == kMaxDistance)
                continue;
            qint16 peerNumber = toShortPeerNumber(commonModule()->moduleGUID(), itr.key());
            serializeCompressPeerNumber(writer, peerNumber);
            writer.putBit(peer.isOnline);
            if (peer.isOnline)
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

void P2pMessageBus::deserializeAlivePeersMessage(
    const P2pConnectionPtr& connection,
    const QByteArray& data)
{
    BitStreamReader reader((const quint8*) data.data(), data.size());
    try
    {
        while (reader.hasMoreBits())
        {
            PeerNumberType peerNumber = deserializeCompressPeerNumber(reader);
            ApiPersistentIdData peerId = fromShortPeerNumber(connection->remotePeer().id, peerNumber);
            bool isOnline = reader.getBit();
            quint32 receivedDistance = 0;
            if (isOnline)
                receivedDistance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            else
                receivedDistance = reader.getBits(32);
            PeerInfo& peerInfo = m_allPeers[peerId];
            if (peerInfo.isOnline && !isOnline)
                continue; //< ignore incoming offline record because we have online data

            RoutingRecord& record = peerInfo.routingInfo[connection->remotePeer()];
            record.distance = std::min(receivedDistance, record.distance);
            record.lastRecvTime = qnSyncTime->currentMSecsSinceEpoch();
        }
    }
    catch (...)
    {
    }
}

void P2pMessageBus::sendAlivePeersMessage()
{
    QByteArray data = serializePeersMessage();
    for (const auto& connection : m_connections)
    {
        if (connection->state() == P2pConnection::State::Connected)
            connection->sendMessage(data);
    }
}

void P2pMessageBus::doSubscribe()
{
    const auto localPeer = this->localPeer();

    QMap<ApiPersistentIdData, std::vector<ApiPersistentIdData>> candidatesToSubscribe; //< key: who, value: where
    for (auto itr = m_allPeers.begin(); itr != m_allPeers.end(); ++itr)
    {
        const ApiPersistentIdData& peer = itr.key();
        const PeerInfo& info = itr.value();
        quint32 directDistance = info.distanceVia(localPeer);
        std::vector<ApiPersistentIdData> viaList;
        quint32 minDistance = info.minDistance(&viaList);
        if (minDistance < directDistance)
        {
            if (P2pConnectionPtr subscribedVia = m_subscriptionList.value(peer))
            {
                quint32 subscribedDistance = info.distanceVia(subscribedVia->remotePeer());
                if (minDistance < subscribedDistance)
                    subscribedVia->unsubscribeFrom(peer);
                else
                    continue; //< already subscribed
            }
            int maxSubscription = 0;
            ApiPersistentIdData peerWithMaxSubscription;
            for (int i = 0; i < viaList.size(); ++i)
            {
                int subscription = candidatesToSubscribe.value(viaList[i]).size();
                if (subscription > maxSubscription)
                {
                    maxSubscription = subscription;
                    peerWithMaxSubscription = viaList[i];
                }
            }
        }
    }

    for (auto itr = candidatesToSubscribe.begin(); itr != candidatesToSubscribe.end(); ++itr)
    {
        P2pConnectionPtr connection = m_connections.value(itr.key().id);
        NX_ASSERT(connection);
        if (connection)
            connection->subscribeTo(itr.value());
    }
}

void P2pMessageBus::doPeriodicTasks()
{
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

} // ec2
