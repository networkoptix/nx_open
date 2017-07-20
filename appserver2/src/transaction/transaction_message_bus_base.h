#pragma once

#include <QtCore/QThread>

#include <nx_ec/data/api_peer_data.h>
#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include "connection_guard_shared_state.h"
#include "transaction.h"
#include <utils/common/enable_multi_thread_direct_connection.h>
#include "transport_connection_info.h"

namespace ec2
{
    class QnJsonTransactionSerializer;
    class QnUbjsonTransactionSerializer;
    class QnAbstractTransactionTransport;

    class ECConnectionNotificationManager;
    namespace detail {
        class QnDbManager;
    }

    class AbstractTransactionMessageBus:
        public QObject,
        public QnCommonModuleAware,
        public EnableMultiThreadDirectConnection<AbstractTransactionMessageBus>
    {
        Q_OBJECT;
    public:
        AbstractTransactionMessageBus(QnCommonModule* commonModule): QnCommonModuleAware(commonModule)
        {
        }
        virtual ~AbstractTransactionMessageBus() {}

        virtual void start() = 0;
        virtual void stop() = 0;

        virtual QSet<QnUuid> directlyConnectedClientPeers() const = 0;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const = 0;
        virtual int distanceToPeer(const QnUuid& dstPeer) const = 0;

        virtual void addOutgoingConnectionToPeer(const QnUuid& id, const QUrl& url) = 0;
        virtual void removeOutgoingConnectionFromPeer(const QnUuid& id) = 0;

        virtual void dropConnections() = 0;

        virtual QVector<QnTransportConnectionInfo> connectionsInfo() const = 0;

        virtual void setHandler(ECConnectionNotificationManager* handler) = 0;
        virtual void removeHandler(ECConnectionNotificationManager* handler) = 0;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const = 0;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const = 0;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() = 0;
        virtual detail::QnDbManager* getDb() const = 0;
        virtual void setTimeSyncManager(TimeSynchronizationManager* timeSyncManager) = 0;
    signals:
        void peerFound(QnUuid data, Qn::PeerType peerType);
        void peerLost(QnUuid data, Qn::PeerType peerType);
        void remotePeerUnauthorized(QnUuid id);

        /** Emitted on a new direct connection to a remote peer has been established */
        void newDirectConnectionEstablished(QnAbstractTransactionTransport* transport);
    };

    class TransactionMessageBusBase: public AbstractTransactionMessageBus
    {
        Q_OBJECT
    public:
        TransactionMessageBusBase(
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
        virtual ~TransactionMessageBusBase();

        virtual void start() override;
        virtual void stop() override;

        virtual void setHandler(ECConnectionNotificationManager* handler) override;
        virtual void removeHandler(ECConnectionNotificationManager* handler) override;

        virtual QnJsonTransactionSerializer* jsonTranSerializer() const override;
        virtual QnUbjsonTransactionSerializer* ubjsonTranSerializer() const override;

        virtual ConnectionGuardSharedState* connectionGuardSharedState() override;
        virtual detail::QnDbManager* getDb() const override { return m_db; }
        virtual void setTimeSyncManager(TimeSynchronizationManager* timeSyncManager) override;

    protected:
        bool readApiFullInfoData(
            const Qn::UserAccessData& userAccess,
            const ec2::ApiPeerData& remotePeer,
            ApiFullInfoData* outData);
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
