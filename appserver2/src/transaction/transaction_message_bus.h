#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

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

        void addConnectionToPeer(const QUrl& url, bool isClient, const QUuid& peer = QUuid());
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, bool isClient, const QnId& removeGuid, qint64 timediff, QList<QByteArray> hwList);
        
        void start();

        template <class T>
        void setHandler(T* handler) { 
            QMutexLocker lock(&m_mutex);
            m_handler = new CustomHandler<T>(handler); 
        }

        template <class T>
        void removeHandler(T* handler) { 
            QMutexLocker lock(&m_mutex);
            if (m_handler->getHandler() == handler) {
                delete m_handler;
                m_handler = 0;
            }
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            QMutexLocker lock(&m_mutex);
            QByteArray buffer;
            m_serializer.serializeTran(buffer, serializedTran, TransactionTransportHeader(peersToSend(tran.command) << qnCommon->moduleGUID()));
            sendTransactionInternal(tran, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const PeerList& dstPeers = PeerList())
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            QMutexLocker lock(&m_mutex);
            QByteArray buffer;
            m_serializer.serializeTran(buffer, tran, TransactionTransportHeader(peersToSend(tran.command) << qnCommon->moduleGUID(), dstPeers));
            sendTransactionInternal(tran, buffer, dstPeers);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnId& dstPeer)
        {
            Q_ASSERT(tran.command != ApiCommand::NotDefined);
            PeerList pList;
            if (!dstPeer.isNull())
                pList << dstPeer;
            sendTransaction(tran, pList);
        }

        struct AlivePeerInfo
        {
            AlivePeerInfo(): isClient(false), isProxy(false) { lastActivity.restart(); }
            AlivePeerInfo(bool isClient, bool isProxy, QList<QByteArray> hwList): isClient(isClient), isProxy(isProxy), hwList(hwList) { lastActivity.restart(); }
            bool isClient;
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

        void sendServerAliveMsg(const ApiServerAliveData &data);

    signals:
        void peerLost(ApiServerAliveData data, bool isProxy);
        void peerFound(ApiServerAliveData data, bool isProxy);

        void incompatiblePeerFound(const ApiServerAliveData &data);
        void incompatiblePeerLost(const ApiServerAliveData &data);

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
        void sendTransactionInternal(const QnAbstractTransaction& tran, const QByteArray& chunkData, const PeerList& dstPeers = PeerList());
        bool onGotTransactionSyncRequest(QnTransactionTransport* sender, QnInputBinaryStream<QByteArray>& stream);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, QnInputBinaryStream<QByteArray>& stream);
        void onGotDistributedMutexTransaction(const QnAbstractTransaction& tran, QnInputBinaryStream<QByteArray>&);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const QnId& id, bool isClient, const QList<QByteArray>& hwList);
        void connectToPeerLost(const QnId& id);
        void sendServerAliveMsg(const QnId& id, bool isAlive, bool isClient, const QList<QByteArray>& hwList);
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnAbstractTransaction& abstractTran, QnInputBinaryStream<QByteArray>& stream);
        QSet<QUuid> peersToSend(ApiCommand::Value command) const;
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(QByteArray serializedTran, TransactionTransportHeader transportHeader);
        void doPeriodicTasks();
    private:
        QnTransactionTransportSerializer m_serializer;
        //typedef QMap<QUrl, QSharedPointer<QnTransactionTransport>> RemoveUrlMap;

        //RemoveUrlMap m_remoteUrls;
        struct RemoteUrlConnectInfo {
            RemoteUrlConnectInfo(bool isClient = false, const QUuid& peer = QUuid()): isClient(isClient), peer(peer), lastConnectedTime(0) {}
            bool isClient;
            QUuid peer;
            qint64 lastConnectedTime;
        };

        QMap<QUrl, RemoteUrlConnectInfo> m_remoteUrls;
        AbstractHandler* m_handler;
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
#define qnTransactionBus ec2::QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
