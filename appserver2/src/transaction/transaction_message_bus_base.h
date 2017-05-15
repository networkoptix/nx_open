#pragma once

#include <QtCore/QThread>

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include "connection_guard_shared_state.h"
#include "transaction.h"
#include <utils/common/enable_multi_thread_direct_connection.h>

namespace ec2
{
    class QnJsonTransactionSerializer;
    class QnUbjsonTransactionSerializer;

    class ECConnectionNotificationManager;
    namespace detail {
        class QnDbManager;
    }

    class QnTransactionMessageBusBase:
        public QObject,
        public QnCommonModuleAware,
        public EnableMultiThreadDirectConnection<QnTransactionMessageBusBase>
    {
        Q_OBJECT
    public:
        QnTransactionMessageBusBase(
            detail::QnDbManager* db,
            Qn::PeerType peerType,
            QnCommonModule* commonModule,
            QnJsonTransactionSerializer* jsonTranSerializer,
            QnUbjsonTransactionSerializer* ubjsonTranSerializer
        );

        struct RoutingRecord
        {
            RoutingRecord(): distance(0), lastRecvTime(0) {}
            RoutingRecord(int distance, qint64 lastRecvTime): distance(distance), lastRecvTime(lastRecvTime) {}

            qint32 distance;
            qint64 lastRecvTime;
        };
        virtual ~QnTransactionMessageBusBase();

        virtual void start();
        virtual void stop();

        virtual QMap<QnUuid, ApiPeerData> aliveServerPeers() const = 0;
        virtual QMap<QnUuid, ApiPeerData> aliveClientPeers(int maxDistance = std::numeric_limits<int>::max()) const = 0;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const = 0;
        virtual int distanceToPeer(const QnUuid& dstPeer) const = 0;

        virtual void addOutgoingConnectionToPeer(const QnUuid& id, const QUrl& url) = 0;
        virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) = 0;

        void setHandler(ECConnectionNotificationManager* handler);
        void removeHandler(ECConnectionNotificationManager* handler);

        QnJsonTransactionSerializer* jsonTranSerializer() const;
        QnUbjsonTransactionSerializer* ubjsonTranSerializer() const;

        ConnectionGuardSharedState* connectionGuardSharedState();
        detail::QnDbManager* getDb() const { return m_db; }
        void setTimeSyncManager(TimeSynchronizationManager* timeSyncManager);

        template<class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
        {
            if (auto p2pBus = dynamic_cast<P2pMessageBus*>(this))
                p2pBus->sendTransaction<T>(tran, dstPeers);
            else if (auto msgBus = dynamic_cast<QnTransactionMessageBus*>(this))
                msgBus->sendTransaction<T>(tran, dstPeers);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnUuid& dstPeerId)
        {
            dstPeerId.isNull() ? sendTransaction(tran) : sendTransaction(tran, QnPeerSet() << dstPeerId);
        }
    signals:
        void peerFound(ApiPeerData data);
        void peerLost(ApiPeerData data);
        void remotePeerUnauthorized(QnUuid id);
    protected:
        detail::QnDbManager* m_db = nullptr;
        QThread* m_thread = nullptr;
        ECConnectionNotificationManager* m_handler = nullptr;

        QnJsonTransactionSerializer* m_jsonTranSerializer = nullptr;
        QnUbjsonTransactionSerializer* m_ubjsonTranSerializer = nullptr;

        /** Info about us. */
        Qn::PeerType m_localPeerType = Qn::PT_NotDefined;

        mutable QnMutex m_mutex;
        ConnectionGuardSharedState m_connectionGuardSharedState;
        TimeSynchronizationManager* m_timeSyncManager = nullptr;
    };
};
