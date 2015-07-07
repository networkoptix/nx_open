#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <memory>

#include <QtCore/QElapsedTimer>
#include <QTime>

#include <common/common_module.h>
#include <utils/common/enable_multi_thread_direct_connection.h>

#include <nx_ec/ec_api.h>
#include "nx_ec/data/api_lock_data.h"
#include <nx_ec/data/api_peer_data.h>
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"
#include "transaction_transport.h"
#include <transaction/transaction_log.h>
#include "runtime_transaction_log.h"
#include "transport_connection_info.h"

#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>


class QTimer;
class QnRuntimeTransactionLog;

namespace ec2
{
    class ECConnectionNotificationManager;

    class QnTransactionMessageBus
    :
        public QObject,
        public EnableMultiThreadDirectConnection<QnTransactionMessageBus>
    {
        Q_OBJECT
    public:
        QnTransactionMessageBus(Qn::PeerType peerType );
        virtual ~QnTransactionMessageBus();

        static QnTransactionMessageBus* instance();

        void addConnectionToPeer(const QUrl& url);
        void removeConnectionFromPeer(const QUrl& url);
        QList<QnTransportConnectionInfo> connectionsInfo() const;
        void gotConnectionFromRemotePeer(
            const QnUuid& connectionGuid,
            QSharedPointer<AbstractStreamSocket> socket,
            ConnectionType::Type connectionType,
            const ApiPeerData& remotePeer,
            qint64 remoteSystemIdentityTime,
            const nx_http::Request& request,
            const QByteArray& contentEncoding );
        //!Report socket to receive transactions from
        /*!
            \param requestBuf Contains serialized \a request and (possibly) partial (or full) message body
        */
        void gotIncomingTransactionsConnectionFromRemotePeer(
            const QnUuid& connectionGuid,
            const QSharedPointer<AbstractStreamSocket>& socket,
            const ApiPeerData &remotePeer,
            qint64 remoteSystemIdentityTime,
            const nx_http::Request& request,
            const QByteArray& requestBuf );
        //!Process transaction received via standard HTTP server interface
        bool gotTransactionFromRemotePeer(
            const QnUuid& connectionGuid,
            const nx_http::Request& request,
            const QByteArray& requestMsgBody );
        //!Blocks till connection \a connectionGuid is ready to accept new transactions
        void waitForNewTransactionsReady( const QnUuid& connectionGuid );
        void connectionFailure( const QnUuid& connectionGuid );
        void dropConnections();
        
        ApiPeerData localPeer() const;

        void start();
        void stop();

        /*!
            \param handler Control of life-time of this object is out of scope of this class
        */
        void setHandler(ECConnectionNotificationManager* handler);

        void removeHandler(ECConnectionNotificationManager* handler);

        template<class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QMutexLocker lock(&m_mutex);
            if (m_connections.isEmpty())
                return;
            QnTransactionTransportHeader ttHeader(connectedServerPeers() << m_localPeer.id, dstPeers);
            ttHeader.fillSequence();
            sendTransactionInternal(tran, ttHeader);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnUuid& dstPeerId)
        {
            dstPeerId.isNull() ? sendTransaction(tran) : sendTransaction(tran, QnPeerSet() << dstPeerId);
        }

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
            AlivePeerInfo(): peer(QnUuid(), QnUuid(), Qn::PT_Server)  {  }
            AlivePeerInfo(const ApiPeerData &peer): peer(peer) { }
            ApiPeerData peer;
            
            RoutingInfo routingInfo; // key: route throw, value - distance in hops
            //QSet<QnUuid> proxyVia;
            //bool directAccess;
        };
        typedef QMap<QnUuid, AlivePeerInfo> AlivePeersMap;

        /*
        * Return all alive peers
        */
        AlivePeersMap alivePeers() const;

        /*
        * Return all alive server peers
        */
        AlivePeersMap aliveServerPeers() const;
        AlivePeersMap aliveClientPeers() const;

        /*
        * Return routing information: how to access to a dstPeer. 
        * if peer can be access directly then return same value as input. 
        * If can't find route info then return null value. 
        * Otherwise return route gateway.
        */
        QnUuid routeToPeerVia(const QnUuid& dstPeer, int* distance) const;

    signals:
        void peerLost(ApiPeerAliveData data);
        //!Emitted when a new peer has joined cluster or became online
        void peerFound(ApiPeerAliveData data);
        //!Emitted on a new direct connection to a remote peer has been established
        void newDirectConnectionEstablished(QnTransactionTransport* transport);

        void gotLockRequest(ApiLockData);
        //void gotUnlockRequest(ApiLockData);
        void gotLockResponse(ApiLockData);

        void remotePeerUnauthorized(const QnUuid& id);
    private:
        friend class QnTransactionTransport;
        friend struct GotTransactionFuction;
        friend struct SendTransactionToTransportFuction;

        bool isExists(const QnUuid& removeGuid) const;
        bool isConnecting(const QnUuid& removeGuid) const;

        typedef QMap<QnUuid, QnTransactionTransport*> QnConnectionMap;

    private:
        template<class T>
        void sendTransactionInternal(const QnTransaction<T>& tran, const QnTransactionTransportHeader &header) {
            QnPeerSet toSendRest = header.dstPeers;
            QnPeerSet sentPeers;
            bool sendToAll = header.dstPeers.isEmpty();

            for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
            {
                QnTransactionTransport* transport = *itr;
                if (!sendToAll && !header.dstPeers.contains(transport->remotePeer().id)) 
                    continue;

                if (!transport->isReadyToSend(tran.command)) 
                    continue;

                transport->sendTransaction(tran, header);
                sentPeers << transport->remotePeer().id;
                toSendRest.remove(transport->remotePeer().id);
            }

            // some dst is not accessible directly, send broadcast (to all connected peers except of just sent)
            if (!toSendRest.isEmpty() && !tran.isLocal)
            {
                for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
                {
                    QnTransactionTransport* transport = *itr;
                    if (!transport->isReadyToSend(tran.command))
                        continue;;

                    if (sentPeers.contains(transport->remotePeer().id))
                        continue; // already sent

                    transport->sendTransaction(tran, header);
                }
            }
        }

        template <class T>
        void sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader);
        
        template <class T>
        void gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader);

        void onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<ApiSyncRequestData> &tran);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, const QnTransaction<QnTranStateResponse> &tran);
        void onGotTransactionSyncDone(QnTransactionTransport* sender, const QnTransaction<ApiTranSyncDoneData> &tran);
        void onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const ApiPeerData &peerInfo);
        void connectToPeerLost(const QnUuid& id);
        void handlePeerAliveChanged(const ApiPeerData& peer, bool isAlive, bool sendTran);
        QnTransaction<ApiModuleDataList> prepareModulesDataTransaction() const;
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader);
        bool onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader& ttHeader);

        /*
        * Return true if alive transaction accepted or false if it should be ignored (offline data is deprecated)
        */
        bool gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport, const QnTransactionTransportHeader* ttHeader);

        QnPeerSet connectedServerPeers() const;

        void sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState);

        void addAlivePeerInfo(const ApiPeerData& peerData, const QnUuid& gotFromPeer, int distance);
        void removeAlivePeer(const QnUuid& id, bool sendTran, bool isRecursive = false);
        bool sendInitialData(QnTransactionTransport* transport);
        void printTranState(const QnTranState& tranState);
        template <class T> void proxyTransaction(const QnTransaction<T> &tran, const QnTransactionTransportHeader &transportHeader);
        void updatePersistentMarker(const QnTransaction<ApiUpdateSequenceData>& tran, QnTransactionTransport* transport);
        void proxyFillerTransaction(const QnAbstractTransaction& tran, const QnTransactionTransportHeader& transportHeader);
        void removeTTSequenceForPeer(const QnUuid& id);
        bool isSyncInProgress() const;
        void removePeersWithTimeout(const QSet<QnUuid>& lostPeers);
        QSet<QnUuid> checkAlivePeerRouteTimeout();
        void updateLastActivity(QnTransactionTransport* sender, const QnTransactionTransportHeader& transportHeader);
        int distanceToPeer(const QnUuid& dstPeer) const;
        void addDelayedAliveTran(QnTransaction<ApiPeerAliveData>&& tranToSend, int timeout);
        void sendDelayedAliveTran();
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(
            Qn::SerializationFormat tranFormat,
            const QByteArray &serializedTran,
            const QnTransactionTransportHeader &transportHeader);
        void doPeriodicTasks();
        bool checkSequence(const QnTransactionTransportHeader& transportHeader, const QnAbstractTransaction& tran, QnTransactionTransport* transport);
        void at_peerIdDiscovered(const QUrl& url, const QnUuid& id);
        void at_runtimeDataUpdated(const QnTransaction<ApiRuntimeData>& data);
        void emitRemotePeerUnauthorized(const QnUuid& id);
    private:
        /** Info about us. */
        const ApiPeerData m_localPeer;

        //QScopedPointer<QnBinaryTransactionSerializer> m_binaryTranSerializer;
        QScopedPointer<QnJsonTransactionSerializer> m_jsonTranSerializer;
		QScopedPointer<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;

        struct RemoteUrlConnectInfo {
            RemoteUrlConnectInfo()  { lastConnectedTime.invalidate(); }
            QElapsedTimer lastConnectedTime;
            QnUuid discoveredPeer;
            QElapsedTimer discoveredTimeout;
        };

        QMap<QUrl, RemoteUrlConnectInfo> m_remoteUrls;
        ECConnectionNotificationManager* m_handler;
        QTimer* m_timer;
        mutable QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;

        AlivePeersMap m_alivePeers;
        QVector<QnTransactionTransport*> m_connectingConnections;

        QMap<QnTranStateKey, int> m_lastTransportSeq;

        // alive control
        QElapsedTimer m_aliveSendTimer;
        QElapsedTimer m_currentTimeTimer;
        std::unique_ptr<QnRuntimeTransactionLog> m_runtimeTransactionLog;
        bool m_restartPending;
        
        struct DelayedAliveData
        {
            QnTransaction<ApiPeerAliveData> tran;
            qint64 timeToSend;
        };

        QMap<QnUuid, DelayedAliveData> m_delayedAliveTran;
        QElapsedTimer m_relativeTimer;
    };
}
#define qnTransactionBus ec2::QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
