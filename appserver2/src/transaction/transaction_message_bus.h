#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <memory>

#include <QtCore/QElapsedTimer>

#include <nx_ec/ec_api.h>
#include "nx_ec/data/api_camera_data.h"
#include "nx_ec/data/api_resource_data.h"
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

    class QnTransactionMessageBus: public QObject
    {
        Q_OBJECT
    public:
        QnTransactionMessageBus();
        virtual ~QnTransactionMessageBus();

        static QnTransactionMessageBus* instance();
        static void initStaticInstance(QnTransactionMessageBus* instance);

        void addConnectionToPeer(const QUrl& url, const QUuid& peer = QUuid());
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QnPeerInfo &remotePeer, QList<QByteArray> hwList);
        
        void setLocalPeer(const QnPeerInfo localPeer);
        QnPeerInfo localPeer() const;

        void start();

        template <class T>
        void setHandler(T* handler) { 
            QMutexLocker lock(&m_mutex);
            m_handler.reset( new CustomHandler<T>(handler) );
        }

        template <class T>
        void removeHandler(T* handler) { 
            QMutexLocker lock(&m_mutex);
            if (m_handler->getHandler() == handler) {
                m_handler.reset();
            }
        }

        template<class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QMutexLocker lock(&m_mutex);
            sendTransactionInternal(tran, QnTransactionTransportHeader(connectedPeers(tran.command) << m_localPeer.id, dstPeers));
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnId& dstPeerId)
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QnPeerSet pSet;
            if (!dstPeerId.isNull())
                pSet << dstPeerId;
            sendTransaction(tran, pSet);
        }

        struct AlivePeerInfo
        {
            AlivePeerInfo(): peer(QnId(), QnPeerInfo::Server), isProxy(false) { lastActivity.restart(); }
            AlivePeerInfo(const QnPeerInfo &peer, bool isProxy, QList<QByteArray> hwList): peer(peer), isProxy(isProxy), hwList(hwList) { lastActivity.restart(); }
            QnPeerInfo peer;
            bool isProxy;
            QElapsedTimer lastActivity;
            QList<QByteArray> hwList;
        };
        typedef QMap<QnId, AlivePeerInfo> AlivePeersMap;

        /*
        * Return all alive peers
        */
        AlivePeersMap alivePeers() const;

        /*
        * Return all alive server peers
        */
        AlivePeersMap aliveServerPeers() const;

    signals:
        void peerLost(ApiPeerAliveData data, bool isProxy);
        void peerFound(ApiPeerAliveData data, bool isProxy);

        void gotLockRequest(ApiLockData);
        //void gotUnlockRequest(ApiLockData);
        void gotLockResponse(ApiLockData);

        void transactionProcessed(const QnAbstractTransaction &transaction);
    private:
        friend class QnTransactionTransport;

        bool isExists(const QnId& removeGuid) const;
        bool isConnecting(const QnId& removeGuid) const;

        class AbstractHandler
        {
        public:
            virtual bool processTransaction(QnTransactionTransport* sender, QnAbstractTransaction& tran, QnInputBinaryStream<QByteArray>& stream) = 0;
            virtual void* getHandler() const = 0;
            virtual ~AbstractHandler() {}
        };

        template <class T>
        class CustomHandler: public AbstractHandler
        {
        public:
            CustomHandler(T* handler): m_handler(handler) {}

            virtual bool processTransaction(QnTransactionTransport* sender, QnAbstractTransaction& tran, QnInputBinaryStream<QByteArray>& stream) override;
            virtual void* getHandler() const override { return m_handler; }
        private:
            template <class T2> bool deliveryTransaction(const QnTransaction<T2> &tran);
        private:
            T* m_handler;
        };

        typedef QMap<QUuid, QSharedPointer<QnTransactionTransport>> QnConnectionMap;

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
        void sendTransactionToTransport(const QnTransaction<T> &tran, QnTransactionTransport* transport, const QnTransactionTransportHeader &transportHeader) {
            transport->sendTransaction(tran, transportHeader);
        }
        
        template <class T>
        void gotTransaction(const QnTransaction<T> &tran, QnTransactionTransport* sender, const QnTransactionTransportHeader &transportHeader) {
            AlivePeersMap:: iterator itr = m_alivePeers.find(tran.id.peerID);
            if (itr != m_alivePeers.end())
                itr.value().lastActivity.restart();

            if (isSystem(tran.command)) {
                if (m_lastTranSeq[tran.id.peerID] >= tran.id.sequence)
                    return; // already processed
                m_lastTranSeq[tran.id.peerID] = tran.id.sequence;
            }

            if (transportHeader.dstPeers.isEmpty() || transportHeader.dstPeers.contains(m_localPeer.id)) {
                qDebug() << "got transaction " << ApiCommand::toString(tran.command) << "with time=" << tran.timestamp;
                // process system transactions
                switch(tran.command) {
                case ApiCommand::lockRequest:
                case ApiCommand::lockResponse:
                case ApiCommand::unlockRequest: 
                    {
                        onGotDistributedMutexTransaction(tran);
                        break;
                    }
                case ApiCommand::clientInstanceId:
                    //TODO: #GDM VW save clientInstanceId to corresponding connection
                    return;
                case ApiCommand::tranSyncRequest:
                    onGotTransactionSyncRequest(sender, tran);
                    return;
                case ApiCommand::tranSyncResponse:
                    onGotTransactionSyncResponse(sender);
                    return;
                case ApiCommand::serverAliveInfo:
                    onGotServerAliveInfo(tran);
                    break; // do not return. proxy this transaction
                default:
                    // general transaction
                    if (!sender->isReadSync())
                        return;

                    if (m_handler && !m_handler->deliveryTransaction(tran)) {
                        qWarning() << "Can't handle transaction" << ApiCommand::toString(tran.command) << "reopen connection";
                        sender->setState(QnTransactionTransport::Error);
                        return;
                    }

                    // this is required to allow client place transactions directly into transaction message bus
                    if (tran.command == ApiCommand::getAllDataList)
                        sender->setWriteSync(true);
                    break;
                }
            }
            else {
                qDebug() << "skip transaction " << ApiCommand::toString(tran.command) << "for peers" << transportHeader.dstPeers;
            }

            QMutexLocker lock(&m_mutex);

            // proxy incoming transaction to other peers.
            if (!transportHeader.dstPeers.isEmpty() && (transportHeader.dstPeers - transportHeader.processedPeers).isEmpty()) {
                emit transactionProcessed(tran);
                return; // all dstPeers already processed
            }

            for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr) {
                QnTransactionTransportPtr transport = *itr;
                if (transportHeader.processedPeers.contains(transport->remotePeer().id) || !transport->isReadyToSend(tran.command)) 
                    continue;

                Q_ASSERT(transport->remotePeer().id != tran.id.peerID);
                transport->sendTransaction(tran, QnTransactionTransportHeader(transportHeader.processedPeers + connectedPeers(tran.command)));
            }

            emit transactionProcessed(tran);
        }


        void onGotTransactionSyncRequest(QnTransactionTransport* sender, const QnTransaction<QnTranState> &tran);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender);
        void onGotDistributedMutexTransaction(const QnTransaction<ApiLockData>& tran);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const QnPeerInfo &peerInfo, const QList<QByteArray>& hwList);
        void connectToPeerLost(const QnId& id);
        void sendServerAliveMsg(const QnPeerInfo& peer, bool isAlive, const QList<QByteArray>& hwList);
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnTransaction<ApiPeerAliveData> &tran);
        QnPeerSet connectedPeers(ApiCommand::Value command) const;
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(const QByteArray &serializedTran, const QnTransactionTransportHeader &transportHeader);
        void doPeriodicTasks();
    private:
        /** Info about us. Should be set before start(). */
        QnPeerInfo m_localPeer;

        QScopedPointer<QnBinaryTransactionSerializer> m_binaryTranSerializer;
        QScopedPointer<QnJsonTransactionSerializer> m_jsonTranSerializer;

        struct RemoteUrlConnectInfo {
            RemoteUrlConnectInfo(const QUuid& peer = QUuid()): peer(peer), lastConnectedTime(0) {}
            QUuid peer;
            qint64 lastConnectedTime;
        };

        QMap<QUrl, RemoteUrlConnectInfo> m_remoteUrls;
        std::unique_ptr<AbstractHandler> m_handler;
        QTimer* m_timer;
        mutable QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;

        AlivePeersMap m_alivePeers;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectionsToRemove;
        QMap<QnId, int> m_lastTranSeq;

        // alive control
        QElapsedTimer m_aliveSendTimer;
    };
}
#define qnTransactionBus QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
