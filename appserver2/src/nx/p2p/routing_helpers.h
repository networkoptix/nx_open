#pragma once

#include "p2p_fwd.h"

namespace nx {
namespace p2p {

struct RoutingRecord
{
    RoutingRecord() = default;
    RoutingRecord(int distance, ec2::ApiPersistentIdData firstVia = ec2::ApiPersistentIdData());

    qint32 distance = 0;
    ec2::ApiPersistentIdData firstVia;
};

typedef QMap<nx::vms::api::PersistentIdData, RoutingRecord> RoutingInfo;

struct AlivePeerInfo
{
    AlivePeerInfo() {}

    qint32 distanceTo(const nx::vms::api::PersistentIdData& peer) const;

    RoutingInfo routeTo; // key: route to, value - distance in hops
};
typedef QMap<nx::vms::api::PersistentIdData, AlivePeerInfo> AlivePeersMap;

struct RouteToPeerInfo
{
    RouteToPeerInfo() {}

    qint32 minDistance(RoutingInfo* outViaList = nullptr) const;
    qint32 distanceVia(const nx::vms::api::PersistentIdData& peer) const;
    const RoutingInfo& routeVia() const { return m_routeVia; }

    void remove(const nx::vms::api::PersistentIdData& id)
    {
        m_routeVia.remove(id);
        m_minDistance = kMaxDistance;
    }
    void insert(const nx::vms::api::PersistentIdData& id, const RoutingRecord& record)
    {
        m_routeVia[id] = record;
        m_minDistance = kMaxDistance;
    }

private:
    RoutingInfo m_routeVia; // key: route via, value - distance in hops
    mutable int m_minDistance = kMaxDistance;
};
typedef QMap<nx::vms::api::PersistentIdData, RouteToPeerInfo> RouteToPeerMap;


struct BidirectionRoutingInfo
{
    BidirectionRoutingInfo(const nx::vms::api::PersistentIdData& localPeer);

    void clear();
    void removePeer(const nx::vms::api::PersistentIdData& via);
    void addRecord(
        const nx::vms::api::PersistentIdData& via,
        const nx::vms::api::PersistentIdData& to,
        const RoutingRecord& record);
    qint32 distanceTo(const nx::vms::api::PersistentIdData& peer, RoutingInfo* outVia = nullptr) const;
    qint32 distanceTo(const QnUuid& peerId, RoutingInfo* outVia = nullptr) const;
    void updateLocalDistance(const nx::vms::api::PersistentIdData& peer, qint32 sequence);

    AlivePeersMap alivePeers; //< alive peers in the system. key - route via, value - route to
    RouteToPeerMap allPeerDistances;  //< vice versa
private:
    void addLocalPeer();
private:
    nx::vms::api::PersistentIdData m_localPeer;
};

} // namespace p2p
} // namespace nx
