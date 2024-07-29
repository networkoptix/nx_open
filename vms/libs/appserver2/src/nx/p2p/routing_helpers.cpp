// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "routing_helpers.h"
#include <utils/common/synctime.h>

namespace nx {
namespace p2p {

using namespace ec2;
using namespace vms::api;

RoutingRecord::RoutingRecord(
    int distance,
    vms::api::PersistentIdData firstVia)
    :
    distance(distance),
    firstVia(firstVia)
{
    NX_ASSERT(!firstVia.isNull() || distance == 0 || distance >= kMaxOnlineDistance);
}


qint32 AlivePeerInfo::distanceTo(const PersistentIdData& peer) const
{
    auto itr = routeTo.find(peer);
    return itr != routeTo.end() ? itr->second.distance : kMaxDistance;
}

qint32 RouteToPeerInfo::minDistance(RoutingInfo* outViaList) const
{
    if (m_minDistance == kMaxDistance)
    {
        for (auto itr = m_routeVia.cbegin(); itr != m_routeVia.cend(); ++itr)
            m_minDistance = std::min(m_minDistance, itr->second.distance);
    }
    if (outViaList)
    {
        for (auto itr = m_routeVia.cbegin(); itr != m_routeVia.cend(); ++itr)
        {
            if (itr->second.distance == m_minDistance)
                outViaList->emplace(itr->first, itr->second);
        }
    }
    return m_minDistance;
}

qint32 RouteToPeerInfo::distanceVia(const PersistentIdData& peer) const
{
    auto itr = m_routeVia.find(peer);
    return itr != m_routeVia.end() ? itr->second.distance : kMaxDistance;
}

// ---------------------- BidirectionRoutingInfo --------------

BidirectionRoutingInfo::BidirectionRoutingInfo(const PersistentIdData& localPeer):
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

void BidirectionRoutingInfo::removePeer(const PersistentIdData& via)
{
    alivePeers.remove(via);
    for (auto itr = allPeerDistances.begin(); itr != allPeerDistances.end(); ++itr)
        itr->remove(via);
}

void BidirectionRoutingInfo::addRecord(
    const PersistentIdData& via,
    const PersistentIdData& to,
    const RoutingRecord& record)
{
    alivePeers[via].routeTo[to] = record;
    allPeerDistances[to].insert(via, record);
}

qint32 BidirectionRoutingInfo::distanceTo(
    const PersistentIdData& peer,
    RoutingInfo* outVia) const
{
    auto itr = allPeerDistances.find(peer);
    return itr != allPeerDistances.end() ? itr->minDistance(outVia) : kMaxDistance;
}

qint32 BidirectionRoutingInfo::distanceTo(
    const nx::Uuid& peerId,
    RoutingInfo* outVia) const
{
    PersistentIdData peer(peerId, nx::Uuid());

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

} // namespace p2p
} // namespace nx
