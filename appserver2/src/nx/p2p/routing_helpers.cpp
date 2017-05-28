#include "routing_helpers.h"
#include <utils/common/synctime.h>

namespace nx {
namespace p2p {

using namespace ec2;

qint32 AlivePeerInfo::distanceTo(const ApiPersistentIdData& peer) const
{
    auto itr = routeTo.find(peer);
    return itr != routeTo.end() ? itr.value().distance : kMaxDistance;
}

qint32 RouteToPeerInfo::minDistance(QVector<ApiPersistentIdData>* outViaList) const
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

qint32 RouteToPeerInfo::distanceVia(const ApiPersistentIdData& peer) const
{
    auto itr = m_routeVia.find(peer);
    return itr != m_routeVia.end() ? itr.value().distance : kMaxDistance;
}

// ---------------------- BidirectionRoutingInfo --------------

BidirectionRoutingInfo::BidirectionRoutingInfo(
    const ApiPersistentIdData& localPeer)
:
    m_localPeer(localPeer)
{
    addLocalPeer();
}

void BidirectionRoutingInfo::clear()
{
    alivePeers.clear();
    allPeerDistances.clear();
    addLocalPeer();
}

void BidirectionRoutingInfo::addLocalPeer()
{
    alivePeers[m_localPeer].routeTo[m_localPeer] = RoutingRecord(0);
    allPeerDistances[m_localPeer].insert(m_localPeer, RoutingRecord(0));
}

void BidirectionRoutingInfo::removePeer(const ApiPersistentIdData& via)
{
    alivePeers.remove(via);
    alivePeers[m_localPeer].routeTo.remove(via);

    allPeerDistances[via].remove(m_localPeer);
    for (auto itr = allPeerDistances.begin(); itr != allPeerDistances.end(); ++itr)
        itr->remove(via);
}

void BidirectionRoutingInfo::addRecord(
    const ApiPersistentIdData& via,
    const ApiPersistentIdData& to,
    const RoutingRecord& record)
{
    alivePeers[via].routeTo[to] = record;
    allPeerDistances[to].insert(via, record);
}

qint32 BidirectionRoutingInfo::distanceTo(
    const ApiPersistentIdData& peer,
    QVector<ApiPersistentIdData>* outVia) const
{
    auto itr = allPeerDistances.find(peer);
    return itr != allPeerDistances.end() ? itr->minDistance(outVia) : kMaxDistance;
}

qint32 BidirectionRoutingInfo::distanceTo(
    const QnUuid& peerId,
    QVector<ApiPersistentIdData>* outVia) const
{
    ApiPersistentIdData peer(peerId, QnUuid());

    qint32 result = kMaxDistance;
    for (auto itr = allPeerDistances.lowerBound(peer);
         itr != allPeerDistances.end() && itr.key().id == peerId;
         ++itr)
    {
        if (itr->minDistance() < result)
        {
            if (outVia)
                outVia->clear();
            result = itr->minDistance(outVia);
        }
    }
    return result;
}

void BidirectionRoutingInfo::updateLocalDistance(const ApiPersistentIdData& peer, qint32 sequence)
{
    return;
    if (peer == m_localPeer)
        return;
    const qint32 localOfflineDistance = kMaxDistance - sequence;
    addRecord(
        m_localPeer,
        peer,
        RoutingRecord(localOfflineDistance));
}

} // namespace p2p
} // namespace nx
