#pragma once

#include <QtCore/QThread>

#include <nx_ec/data/api_peer_data.h>
#include <common/common_module_aware.h>
#include "connection_guard_shared_state.h"
#include <utils/common/enable_multi_thread_direct_connection.h>
#include "transport_connection_info.h"

namespace ec2
{
    class QnJsonTransactionSerializer;
    class QnUbjsonTransactionSerializer;
    class QnAbstractTransactionTransport;
    class TimeSynchronizationManager;

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
        AbstractTransactionMessageBus(QnCommonModule* value): QnCommonModuleAware(value) {}
        virtual ~AbstractTransactionMessageBus() {}

        virtual void start() = 0;
        virtual void stop() = 0;

        virtual QSet<QnUuid> directlyConnectedClientPeers() const = 0;

        virtual QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const = 0;
        virtual int distanceToPeer(const QnUuid& dstPeer) const = 0;

        virtual void addOutgoingConnectionToPeer(const QnUuid& id, const nx::utils::Url& url) = 0;
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
};
