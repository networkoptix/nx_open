#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <memory>

#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api.h>
#include "nx_ec/data/api_lock_data.h"
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"
#include "transaction_transport.h"
#include <transaction/transaction_log.h>
#include "common/common_module.h"

#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>

class QTimer;

namespace ec2
{
    class ECConnectionNotificationManager;

    class QnTransactionMessageBus: public QObject
    {
        Q_OBJECT
    public:
        QnTransactionMessageBus();
        virtual ~QnTransactionMessageBus();

        static QnTransactionMessageBus* instance();

        void addConnectionToPeer(const QUrl& url, const QUuid& peer = QUuid());
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(const QSharedPointer<AbstractStreamSocket>& socket, const ApiPeerData &remotePeer);
        void dropConnections();
        
        void setLocalPeer(const ApiPeerData localPeer);
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
            QnTransactionTransportHeader ttHeader(connectedPeers(tran.command) << m_localPeer.id, dstPeers);
            ttHeader.fillSequence();
            sendTransactionInternal(tran, ttHeader);
        }

        /** Template specialization to fill dstPeers from the transaction params. */
        void sendTransaction(const QnTransaction<ec2::ApiVideowallControlMessageData>& tran, const QnPeerSet& dstPeers = QnPeerSet());

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QUuid& dstPeerId)
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QnPeerSet pSet;
            if (!dstPeerId.isNull())
                pSet << dstPeerId;
            sendTransaction(tran, pSet);
        }

        struct AlivePeerInfo
        {
            AlivePeerInfo(): peer(QUuid(), Qn::PT_Server) { lastActivity.restart(); }
            AlivePeerInfo(const ApiPeerData &peer): peer(peer) { lastActivity.restart(); }
            ApiPeerData peer;
            QSet<QUuid> proxyVia;
            QElapsedTimer lastActivity;
        };
        typedef QMap<QUuid, AlivePeerInfo> AlivePeersMap;

        /*
        * Return all alive peers
        */
        AlivePeersMap alivePeers() const;

        /*
        * Return all alive server peers
        */
        AlivePeersMap aliveServerPeers() const;

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

    private:
        friend class QnTransactionTransport;
        friend struct GotTransactionFuction;
        friend struct SendTransactionToTransportFuction;

        bool isExists(const QUuid& removeGuid) const;
        bool isConnecting(const QUuid& removeGuid) const;

        typedef QMap<QUuid, QnTransactionTransportPtr> QnConnectionMap;

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

        void onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<QnTranState> &tran);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, const QnTransaction<QnTranStateResponse> &tran);
        void onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const ApiPeerData &peerInfo);
        void connectToPeerLost(const QUuid& id);
        void handlePeerAliveChanged(const ApiPeerData& peer, bool isAlive, bool sendTran);
        QnTransaction<ApiModuleDataList> prepareModulesDataTransaction() const;
        void sendConnectionsData();
        void sendModulesData();
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran, const QUuid& gotFromID);
        void onGotServerRuntimeInfo(const QnTransaction<ApiRuntimeData> &tran, const QUuid& gotFromPeer);
        void processAliveData(const ApiPeerAliveData &aliveData, const QUuid& gotFromPeer);

        QnPeerSet connectedPeers(ApiCommand::Value command) const;

        void sendRuntimeInfo(QnTransactionTransport* transport, const QnTransactionTransportHeader& transportHeader);

        void addAlivePeerInfo(ApiPeerData peerData, const QUuid& gotFromPeer = QUuid());
        void removeAlivePeer(const QUuid& id, bool sendTran, bool isRecursive = false);
        bool doHandshake(QnTransactionTransport* transport);
        void printTranState(const QnTranState& tranState);
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader);
        void doPeriodicTasks();

    private:
        /** Info about us. Should be set before start(). */
        ApiPeerData m_localPeer;

        QScopedPointer<QnBinaryTransactionSerializer> m_binaryTranSerializer;
        QScopedPointer<QnJsonTransactionSerializer> m_jsonTranSerializer;
		QScopedPointer<QnUbjsonTransactionSerializer> m_ubjsonTranSerializer;

        struct RemoteUrlConnectInfo {
            RemoteUrlConnectInfo(const QUuid& peer = QUuid()): peer(peer), lastConnectedTime(0) {}
            QUuid peer;
            qint64 lastConnectedTime;
        };

        QMap<QUrl, RemoteUrlConnectInfo> m_remoteUrls;
        ECConnectionNotificationManager* m_handler;
        QTimer* m_timer;
        mutable QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;

        AlivePeersMap m_alivePeers;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
        QMap<QUuid, int> m_lastTranSeq;

        // alive control
        QElapsedTimer m_aliveSendTimer;
    };
}
#define qnTransactionBus ec2::QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
