#pragma once

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>

namespace ec2
{
    class QnTransactionMessageBusBase: public QnCommonModuleAware
    {
    public:
        QnTransactionMessageBusBase(QnCommonModule* commonModule);

        struct RoutingRecord
        {
            RoutingRecord(): distance(0), lastRecvTime(0) {}
            RoutingRecord(int distance, qint64 lastRecvTime): distance(distance), lastRecvTime(lastRecvTime) {}

            int distance;
            qint64 lastRecvTime;
        };

        typedef QMap<QnUuid, RoutingRecord> RoutingInfo;
        struct AlivePeerInfo
        {
            AlivePeerInfo() : peer(QnUuid(), QnUuid(), Qn::PT_Server) {}
            AlivePeerInfo(const ApiPeerData &peer) : peer(peer) {}
            ApiPeerData peer;

            RoutingInfo routingInfo; // key: route throw, value - distance in hops
        };
        typedef QMap<QnUuid, AlivePeerInfo> AlivePeersMap;

    public:
        /*
         * Return routing information: how to access to a dstPeer.
         * if peer can be access directly then return same value as input.
         * If can't find route info then return null value.
         * Otherwise return route gateway.
         */
        QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const;

        int distanceToPeer(const QnUuid& dstPeer) const;
    protected:
        mutable QnMutex m_mutex;
        AlivePeersMap m_alivePeers; //< all peers in a system
    };
};
