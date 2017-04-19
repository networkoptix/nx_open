#include "p2p_message_bus.h"
#include <common/common_module.h>
#include <utils/media/bitStream.h>
#include <utils/media/nalUnits.h>
#include <utils/common/synctime.h>

namespace ec2 {

P2pMessageBus::P2pMessageBus(QnCommonModule* commonModule):
    QnTransactionMessageBusBase(commonModule)
{
}

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
    if (itr != m_connections.end() && itr.value()->originatorId() == commonModule()->moduleGUID())
        m_connections.erase(itr);
}

void P2pMessageBus::gotConnectionFromRemotePeer(P2pConnectionPtr connection)
{
    QnMutexLocker lock(&m_mutex);

    const auto remoteId = connection->remoteId();
    const auto localId = commonModule()->moduleGUID();
    auto itr = m_connections.find(remoteId);
    if (itr == m_connections.end())
    {
        // Got incoming connection. No outgoing connection was done yet
        m_connections.insert(remoteId, connection);
    }
    else if (remoteId > localId)
    {
        // Outgoing connection is present, but incoming connection has bigger priority.
        // Replace it.
        itr.value() = connection;
    }
    else if (itr.value()->state() != P2pConnection::State::Connected)
    {
        // Incoming connection has lower priority, but outgoing connection is not done yet
        // If outgoing connection will success it'll replace incoming connection later
        m_outgoingConnections.insert(remoteId, itr.value());
        itr.value() = connection; //< start using incoming connection unless outgoing will OK.
    }
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
            m_connections[connection->remoteId()] = connection;
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
            m_connections.insert(remoteId, P2pConnectionPtr(new P2pConnection(remoteId, url)));
        }
    }
}

PeerNumberType P2pMessageBus::toShortPeerNumber(const QnUuid& owner, const ApiPeerData& peer)
{
    PeerNumberInfo& info = m_shortPeersMap[owner];
    auto itr = info.fullIdToShortId.find(peer);
    if (itr != info.fullIdToShortId.end())
        return itr.value();
    if (owner == commonModule()->moduleGUID())
        return info.insert(peer);
    return kUnknownPeerNumnber;
}

QnUuid P2pMessageBus::fromShortPeerNumber(const PeerNumberType& id)
{
    return QnUuid(); // implement me
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

QByteArray P2pMessageBus::serializeAlivePeersMessage(const AlivePeersMap& peers)
{
    QByteArray result;
    result.resize(peers.size() * 4);
    BitStreamWriter writer;
    writer.setBuffer((quint8*) result.data(), result.size());
    try
    {
        for (const auto& peer: peers.values())
        {
            int minDistance = std::numeric_limits<int>::max();
            for (const RoutingRecord& rec : peer.routingInfo)
                minDistance = std::min(minDistance, rec.distance);
            if (minDistance == std::numeric_limits<int>::max())
                continue;
            qint16 peerNumber = toShortPeerNumber(commonModule()->moduleGUID(), peer.peer);
            serializeCompressPeerNumber(writer, peerNumber);
            NALUnit::writeUEGolombCode(writer, minDistance); // todo: move function to another place
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
            QnUuid peerId = fromShortPeerNumber(peerNumber);
            int receivedDistance = NALUnit::extractUEGolombCode(reader); // todo: move function to another place
            RoutingRecord& record = m_alivePeers[peerId].routingInfo[connection->remoteId()];
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
    QByteArray data = serializeAlivePeersMessage(m_alivePeers);
    for (const auto& connection: m_connections)
        connection->sendMessage(MessageType::alivePeers, data);
}

void P2pMessageBus::doPeriodicTasks()
{
    processTemporaryOutgoingConnections();
    removeClosedConnections();
    createOutgoingConnections();
    sendAlivePeersMessage();
}

} // ec2
