#include "routing_helpers.h"
#include <utils/common/synctime.h>

namespace nx {
namespace p2p {

using namespace ec2;

qint32 AlivePeerInfo::distanceTo(const ApiPersistentIdData& via) const
{
    auto itr = routeTo.find(via);
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

// ---------------------- BidirectionRoutingInfo --------------

BidirectionRoutingInfo::BidirectionRoutingInfo(
    const ApiPersistentIdData& localPeer)
:
    m_localPeer(localPeer)
{
    auto& peerData = alivePeers[localPeer].routeTo[localPeer] = RoutingRecord(0, 0);
    allPeerDistances[localPeer].insert(localPeer, RoutingRecord(0, 0));
}

void BidirectionRoutingInfo::clear()
{
    alivePeers.clear();
    allPeerDistances.clear();
}

void BidirectionRoutingInfo::removePeer(const ApiPersistentIdData& id)
{
    alivePeers.remove(id);
    alivePeers[m_localPeer].routeTo.remove(id);

    allPeerDistances[id].remove(m_localPeer);
    for (auto itr = allPeerDistances.begin(); itr != allPeerDistances.end(); ++itr)
        itr->remove(id);
}

void BidirectionRoutingInfo::addRecord(
    const ApiPersistentIdData& via,
    const ApiPersistentIdData& to,
    const RoutingRecord& record)
{
    alivePeers[via].routeTo[to] = record;
    allPeerDistances[to].insert(via, record);
}

qint32 BidirectionRoutingInfo::distanceTo(const ApiPersistentIdData& peer) const
{
    qint32 result = kMaxDistance;
    for (const auto& alivePeer : alivePeers)
        result = std::min(result, alivePeer.distanceTo(peer));
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
        RoutingRecord(localOfflineDistance, qnSyncTime->currentMSecsSinceEpoch()));
}

} // namespace p2p
} // namespace nx
