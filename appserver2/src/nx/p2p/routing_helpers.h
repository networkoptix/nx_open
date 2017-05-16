#pragma once

#include "p2p_fwd.h"
#include <nx_ec/data/api_peer_data.h>

namespace nx {
namespace p2p {

using namespace ec2;

struct RoutingRecord
{
    RoutingRecord() : distance(0), lastRecvTime(0) {}
    RoutingRecord(int distance, qint64 lastRecvTime) : distance(distance), lastRecvTime(lastRecvTime) {}

    qint32 distance;
    qint64 lastRecvTime;
};

typedef QMap<ApiPersistentIdData, RoutingRecord> RoutingInfo;

struct AlivePeerInfo
{
    AlivePeerInfo() {}

    qint32 distanceTo(const ApiPersistentIdData& via) const;

    RoutingInfo routeTo; // key: route to, value - distance in hops
};
typedef QMap<ApiPersistentIdData, AlivePeerInfo> AlivePeersMap;

struct RouteToPeerInfo
{
    RouteToPeerInfo() {}

    qint32 minDistance(QVector<ApiPersistentIdData>* outViaList = nullptr) const;
    const RoutingInfo& routeVia() const { return m_routeVia; }

    void remove(const ApiPersistentIdData& id)
    {
        m_routeVia.remove(id);
        m_minDistance = kMaxDistance;
    }
    void insert(const ApiPersistentIdData& id, const RoutingRecord& record)
    {
        m_routeVia[id] = record;
        m_minDistance = kMaxDistance;
    }

private:
    RoutingInfo m_routeVia; // key: route via, value - distance in hops
    mutable int m_minDistance = kMaxDistance;
};
typedef QMap<ApiPersistentIdData, RouteToPeerInfo> RouteToPeerMap;


struct BidirectionRoutingInfo
{
    BidirectionRoutingInfo(const ApiPersistentIdData& localPeer);

    void clear();
    void removePeer(const ApiPersistentIdData& id);
    void addRecord(
        const ApiPersistentIdData& via,
        const ApiPersistentIdData& to,
        const RoutingRecord& record);
    qint32 distanceTo(const ApiPersistentIdData& peer) const;
    void updateLocalDistance(const ApiPersistentIdData& peer, qint32 sequence);

    AlivePeersMap alivePeers; //< alive peers in the system. key - route via, value - route to
    RouteToPeerMap allPeerDistances;  //< vice versa
private:
    ApiPersistentIdData m_localPeer;
};

} // namespace p2p
} // namespace nx
