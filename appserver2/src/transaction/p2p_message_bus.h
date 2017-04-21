#pragma once

#include <QtCore/QTimer>

#include <memory>
#include <nx/utils/uuid.h>
#include <common/common_module_aware.h>
#include <nx/utils/thread/mutex.h>
#include <transaction/transaction_message_bus_base.h>
#include <nx_ec/data/api_tran_state_data.h>
#include "p2p_connection.h"
#include "p2p_fwd.h"

namespace ec2 {

    namespace detail {
        class QnDbManager;
    }

    using PeerNumberType = quint16;
    const static PeerNumberType kUnknownPeerNumnber = 0xffff;

    using ResolvePeerNumberMessageType = std::vector<PeerNumberType>;

    struct AlivePeersRecord
    {
        PeerNumberType peerNumber = 0;
        quint16 distance = 0;
    };
    using AlivePeersMessagType = std::vector<AlivePeersRecord>;

    struct SubscribeForDataUpdateRecord
    {
        PeerNumberType peerNumber = 0;
        int sequence = 0;
    };
    using SubscribeForDataUpdatesMessageType = std::vector<SubscribeForDataUpdateRecord>;

    class P2pMessageBus:
        public QObject,
        public QnTransactionMessageBusBase
    {
    public:
        P2pMessageBus(
            detail::QnDbManager* db,
            Qn::PeerType peerType,
            QnCommonModule* commonModule);
        virtual ~P2pMessageBus();

        void gotConnectionFromRemotePeer(P2pConnectionPtr connection);

        void addOutgoingConnectionToPeer(QnUuid& id, const QUrl& url);
        void removeOutgoingConnectionFromPeer(QnUuid& id);

        // Self peer information
        ApiPeerData localPeer() const;

        void start();
    private:
        void doPeriodicTasks();
        void processTemporaryOutgoingConnections();
        void removeClosedConnections();
        void createOutgoingConnections();
        void sendAlivePeersMessage();
        QByteArray serializePeersMessage();
        void deserializeAlivePeersMessage(
            const P2pConnectionPtr& connection,
            const QByteArray& data);

        PeerNumberType toShortPeerNumber(const QnUuid& owner, const ApiPeerIdData& peer);
        ApiPeerIdData fromShortPeerNumber(const QnUuid& owner, const PeerNumberType& id);

        void addOwnfInfoToPeerList();
        void addOfflinePeersFromDb();
        void doSubscribe();
    private:
        QMap<QnUuid, P2pConnectionPtr> m_connections; //< Actual connection list
        QMap<QnUuid, P2pConnectionPtr> m_outgoingConnections; //< Temporary list of outgoing connections
        QMap<QnUuid, QUrl> m_remoteUrls; //< Url list for outgoing connections

        struct PeerNumberInfo
        {
            PeerNumberType insert(const ApiPeerIdData& peer);

            QMap<ApiPeerIdData, PeerNumberType> fullIdToShortId;
            QMap<PeerNumberType, ApiPeerIdData> shortIdToFullId;
        };

        // key - got via peer, value - short numbers
        QMap<QnUuid, PeerNumberInfo> m_shortPeersMap;

        typedef QMap<ApiPeerIdData, RoutingRecord> RoutingInfo;
        struct PeerInfo
        {
            PeerInfo() {}

            quint32 distanceVia(const ApiPeerIdData& via) const;
            quint32 minDistance(std::vector<ApiPeerIdData>* outViaList) const;

            bool isOnline = false;
            RoutingInfo routingInfo; // key: route throw, value - distance in hops
        };
        typedef QMap<ApiPeerIdData, PeerInfo> PeersMap;

        PeersMap m_allPeers; //< all peers in a system

        QMap<ApiPeerIdData, P2pConnectionPtr> m_subscriptionList;
        QThread* m_thread = nullptr;
        QTimer* m_timer = nullptr;
    };
} // ec2
