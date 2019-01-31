#pragma once

#include "p2p_fwd.h"

#include <QtCore/QMap>

namespace nx {
namespace p2p {

struct RoutingRecord
{
    RoutingRecord() = default;
    RoutingRecord(int distance, vms::api::PersistentIdData firstVia = vms::api::PersistentIdData());

    qint32 distance = 0;
    vms::api::PersistentIdData firstVia;
};

using RoutingInfo = QMap<vms::api::PersistentIdData, RoutingRecord>;

struct AlivePeerInfo
{
    AlivePeerInfo() {}
    qint32 distanceTo(const vms::api::PersistentIdData& peer) const;

    RoutingInfo routeTo; // key: route to, value - distance in hops
};

using AlivePeersMap = QMap<vms::api::PersistentIdData, AlivePeerInfo>;

struct RouteToPeerInfo
{
    RouteToPeerInfo() {}

    qint32 minDistance(RoutingInfo* outViaList = nullptr) const;
    qint32 distanceVia(const vms::api::PersistentIdData& peer) const;
    const RoutingInfo& routeVia() const { return m_routeVia; }

    void remove(const vms::api::PersistentIdData& id)
    {
        m_routeVia.remove(id);
        m_minDistance = kMaxDistance;
    }

    void insert(const vms::api::PersistentIdData& id, const RoutingRecord& record)
    {
        m_routeVia[id] = record;
        m_minDistance = kMaxDistance;
    }

private:
    RoutingInfo m_routeVia; // key: route via, value - distance in hops
    mutable int m_minDistance = kMaxDistance;
};

using RouteToPeerMap = QMap<vms::api::PersistentIdData, RouteToPeerInfo>;

struct BidirectionRoutingInfo
{
    BidirectionRoutingInfo(const vms::api::PersistentIdData& localPeer);

    void clear();
    void removePeer(const vms::api::PersistentIdData& via);
    void addRecord(
        const vms::api::PersistentIdData& via,
        const vms::api::PersistentIdData& to,
        const RoutingRecord& record);

    qint32 distanceTo(const vms::api::PersistentIdData& peer, RoutingInfo* outVia = nullptr) const;
    qint32 distanceTo(const QnUuid& peerId, RoutingInfo* outVia = nullptr) const;
    void updateLocalDistance(const vms::api::PersistentIdData& peer, qint32 sequence);

    AlivePeersMap alivePeers; //< alive peers in the system. key - route via, value - route to
    RouteToPeerMap allPeerDistances;  //< vice versa

private:
    void addLocalPeer();

private:
    vms::api::PersistentIdData m_localPeer;
};

} // namespace p2p
} // namespace nx
