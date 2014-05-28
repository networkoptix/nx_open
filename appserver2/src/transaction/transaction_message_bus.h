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
#include "transaction_transport_serializer.h"
#include "transaction_transport.h"
#include "common/common_module.h"

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

        void sendTransaction(const QnAbstractTransaction& tran, const QByteArray& serializedTran)
        {
            QMutexLocker lock(&m_mutex);
            QByteArray buffer;
            m_serializer.serializeTran(buffer, serializedTran, TransactionTransportHeader(connectedPeers(tran.command) << m_localPeer.id));
            sendTransactionInternal(tran, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnPeerSet& dstPeers = QnPeerSet())
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QMutexLocker lock(&m_mutex);
            QByteArray buffer;
            m_serializer.serializeTran(buffer, tran, TransactionTransportHeader(connectedPeers(tran.command) << m_localPeer.id, dstPeers));
            sendTransactionInternal(tran, buffer, dstPeers);
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
            template <class T2> bool deliveryTransaction(const QnAbstractTransaction&  abstractTran, QnInputBinaryStream<QByteArray>& stream);
        private:
            T* m_handler;
        };

        typedef QMap<QUuid, QSharedPointer<QnTransactionTransport>> QnConnectionMap;

    private:
        //void gotTransaction(const QnId& remoteGuid, bool isConnectionOriginator, const QByteArray& data);
        void sendTransactionInternal(const QnAbstractTransaction& tran, const QByteArray& chunkData, const QnPeerSet& dstPeers = QnPeerSet());
        bool onGotTransactionSyncRequest(QnTransactionTransport* sender, QnInputBinaryStream<QByteArray>& stream);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, QnInputBinaryStream<QByteArray>& stream);
        void onGotDistributedMutexTransaction(const QnAbstractTransaction& tran, QnInputBinaryStream<QByteArray>&);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const QnPeerInfo &peerInfo, const QList<QByteArray>& hwList);
        void connectToPeerLost(const QnId& id);
        void sendServerAliveMsg(const QnPeerInfo& peer, bool isAlive, const QList<QByteArray>& hwList);
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnAbstractTransaction& abstractTran, QnInputBinaryStream<QByteArray>& stream);
        QnPeerSet connectedPeers(ApiCommand::Value command) const;
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(QByteArray serializedTran, TransactionTransportHeader transportHeader);
        void doPeriodicTasks();
    private:
        /** Info about us. Should be set before start(). */
        QnPeerInfo m_localPeer;

        QnTransactionTransportSerializer m_serializer;
        //typedef QMap<QUrl, QSharedPointer<QnTransactionTransport>> RemoveUrlMap;

        //RemoveUrlMap m_remoteUrls;
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
