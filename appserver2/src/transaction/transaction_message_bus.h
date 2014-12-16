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

#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>


class QTimer;

namespace ec2
{
    class ECConnectionNotificationManager;

    class QnTransactionMessageBus
    :
        public QObject,
        public EnableMultiThreadDirectConnection
    {
        Q_OBJECT
    public:
        QnTransactionMessageBus(Qn::PeerType peerType );
        virtual ~QnTransactionMessageBus();

        static QnTransactionMessageBus* instance();

        void addConnectionToPeer(const QUrl& url);
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(const QSharedPointer<AbstractStreamSocket>& socket, const ApiPeerData &remotePeer);
        void dropConnections();
        
        ApiPeerData localPeer() const;

        void start();

        /*!
            \param handler Control of life-time of this object is out of scope of this class
        */
        void setHandler(ECConnectionNotificationManager* handler) { 
            QMutexLocker lock(&m_mutex);
            m_handler = handler;
        }

        void removeHandler(ECConnectionNotificationManager* handler) { 
            QMutexLocker lock(&m_mutex);
            if( m_handler == handler )
                m_handler = nullptr;
        }

        template<class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QMutexLocker lock(&m_mutex);
            if (m_connections.isEmpty())
                return;
            QnTransactionTransportHeader ttHeader(connectedPeers(tran.command) << m_localPeer.id, dstPeers);
            ttHeader.fillSequence();
            sendTransactionInternal(tran, ttHeader);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnUuid& dstPeerId)
        {
            dstPeerId.isNull() ? sendTransaction(tran) : sendTransaction(tran, QnPeerSet() << dstPeerId);
        }

        struct AlivePeerInfo
        {
            AlivePeerInfo(): peer(QnUuid(), QnUuid(), Qn::PT_Server), directAccess(false) { lastActivity.restart(); }
            AlivePeerInfo(const ApiPeerData &peer): peer(peer), directAccess(false) { lastActivity.restart(); }
            ApiPeerData peer;
            QSet<QnUuid> proxyVia;
            QElapsedTimer lastActivity;
            bool directAccess;
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

    signals:
        void peerLost(ApiPeerAliveData data);
        //!Emitted when a new peer has joined cluster or became online
        void peerFound(ApiPeerAliveData data);
        //!Emitted on a new direct connection to a remote peer has been established
        void newDirectConnectionEstablished(const QnTransactionTransportPtr& transport);

        void gotLockRequest(ApiLockData);
        //void gotUnlockRequest(ApiLockData);
        void gotLockResponse(ApiLockData);

        void transactionProcessed(const QnAbstractTransaction &transaction);
        void remotePeerUnauthorized(const QnUuid& id);
    private:
        friend class QnTransactionTransport;
        friend struct GotTransactionFuction;
        friend struct SendTransactionToTransportFuction;

        bool isExists(const QnUuid& removeGuid) const;
        bool isConnecting(const QnUuid& removeGuid) const;

        typedef QMap<QnUuid, QnTransactionTransportPtr> QnConnectionMap;

    private:
        template<class T>
        void sendTransactionInternal(const QnTransaction<T>& tran, const QnTransactionTransportHeader &header) {
            QnPeerSet toSendRest = header.dstPeers;
            QnPeerSet sentPeers;
            bool sendToAll = header.dstPeers.isEmpty();

            for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
            {
                QnTransactionTransportPtr transport = *itr;
                if (!sendToAll && !header.dstPeers.contains(transport->remotePeer().id)) 
                    continue;

                if (!transport->isReadyToSend(tran.command)) 
                    continue;

                transport->sendTransaction(tran, header);
                sentPeers << transport->remotePeer().id;
                toSendRest.remove(transport->remotePeer().id);
            }

            // some dst is not accessible directly, send broadcast (to all connected peers except of just sent)
            if (!toSendRest.isEmpty()) 
            {
                for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
                {
                    QnTransactionTransportPtr transport = *itr;
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
        bool onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, QnTransactionTransport* transport);

        /*
        * Return true if alive transaction accepted or false if it should be ignored (offline data is deprecated)
        */
        bool gotAliveData(const ApiPeerAliveData &aliveData, QnTransactionTransport* transport);

        QnPeerSet connectedPeers(ApiCommand::Value command) const;

        void sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader, const QnTranState& runtimeState);

        void addAlivePeerInfo(ApiPeerData peerData, const QnUuid& gotFromPeer = QnUuid());
        void removeAlivePeer(const QnUuid& id, bool sendTran, bool isRecursive = false);
        bool sendInitialData(QnTransactionTransport* transport);
        void printTranState(const QnTranState& tranState);
        template <class T> void proxyTransaction(const QnTransaction<T> &tran, const QnTransactionTransportHeader &transportHeader);
        void updatePersistentMarker(const QnTransaction<ApiUpdateSequenceData>& tran, QnTransactionTransport* transport);
        void proxyFillerTransaction(const QnAbstractTransaction& tran, const QnTransactionTransportHeader& transportHeader);
        void removeTTSequenceForPeer(const QnUuid& id);
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader);
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
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;

        QMap<QnTranStateKey, int> m_lastTransportSeq;

        // alive control
        QElapsedTimer m_aliveSendTimer;
    };
}
#define qnTransactionBus ec2::QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
