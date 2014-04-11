#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <QElapsedTimer>
#include <nx_ec/ec_api.h>
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"
#include "transaction_transport_serializer.h"
#include "transaction_transport.h"
#include "nx_ec/data/ec2_lock_data.h"
#include "common/common_module.h"

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

        void addConnectionToPeer(const QUrl& url, bool isClient);
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, bool isClient, const QnId& removeGuid, qint64 timediff);
        
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
            m_serializer.serializeTran(buffer, serializedTran, peersToSend(tran.command) << qnCommon->moduleGUID());
            sendTransactionInternal(tran, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QnId& dstPeer = QnId())
        {
            QMutexLocker lock(&m_mutex);
            QByteArray buffer;
            m_serializer.serializeTran(buffer, tran, peersToSend(tran.command) << qnCommon->moduleGUID());
            sendTransactionInternal(tran, buffer, dstPeer);
        }

        struct AlivePeerInfo
        {
            AlivePeerInfo(bool isClient = false, bool isProxy = false): isClient(isClient), isProxy(isProxy) { lastActivity.restart(); }
            bool isClient;
            bool isProxy;
            QElapsedTimer lastActivity;
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
        void peerLost(QnId, bool isClient, bool isProxy);
        void peerFound(QnId, bool isClient, bool isProxy);

        void gotLockRequest(ApiLockData);
        void gotUnlockRequest(ApiLockData);
        void gotLockResponse(ApiLockData);
    private:
        friend class QnTransactionTransport;

        bool isExists(const QnId& removeGuid) const;
        bool isConnecting(const QnId& removeGuid) const;

        class AbstractHandler
        {
        public:
            virtual bool processTransaction(QnTransactionTransport* sender, QnAbstractTransaction& tran, InputBinaryStream<QByteArray>& stream) = 0;
            virtual void* getHandler() const = 0;
            virtual ~AbstractHandler() {}
        };

        template <class T>
        class CustomHandler: public AbstractHandler
        {
        public:
            CustomHandler(T* handler): m_handler(handler) {}

            virtual bool processTransaction(QnTransactionTransport* sender, QnAbstractTransaction& tran, InputBinaryStream<QByteArray>& stream) override;
            virtual void* getHandler() const override { return m_handler; }
        private:
            template <class T2> bool deliveryTransaction(const QnAbstractTransaction&  abstractTran, InputBinaryStream<QByteArray>& stream);
        private:
            T* m_handler;
        };

        typedef QMap<QUuid, QSharedPointer<QnTransactionTransport>> QnConnectionMap;

    private:
        //void gotTransaction(const QnId& remoteGuid, bool isConnectionOriginator, const QByteArray& data);
        void sendTransactionInternal(const QnAbstractTransaction& tran, const QByteArray& chunkData, const QnId& dstPeer = QnId());
        bool onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream);
        void onGotDistributedMutexTransaction(const QnAbstractTransaction& tran, InputBinaryStream<QByteArray>&, bool *needProxy);
        void queueSyncRequest(QnTransactionTransport* transport);

        void connectToPeerEstablished(const QnId& id, bool isClient);
        void connectToPeerLost(const QnId& id);
        void sendServerAliveMsg(const QnId& id, bool isAlive, bool isClient);
        bool isPeerUsing(const QUrl& url);
        void onGotServerAliveInfo(const QnAbstractTransaction& abstractTran, InputBinaryStream<QByteArray>& stream);
        QSet<QUuid> peersToSend(ApiCommand::Value command) const;
    private slots:
        void at_stateChanged(QnTransactionTransport::State state);
        void at_timer();
        void at_gotTransaction(QByteArray serializedTran, QSet<QnId> processedPeers);
        void doPeriodicTasks();
    private:
        QnTransactionTransportSerializer m_serializer;
        //typedef QMap<QUrl, QSharedPointer<QnTransactionTransport>> RemoveUrlMap;

        //RemoveUrlMap m_remoteUrls;
        struct RemoveUrlConnectInfo {
            RemoveUrlConnectInfo(bool isClient = false): isClient(isClient), lastConnectedTime(0) {}
            bool isClient;
            qint64 lastConnectedTime;
        };

        QMap<QUrl, RemoveUrlConnectInfo> m_removeUrls;
        AbstractHandler* m_handler;
        QTimer* m_timer;
        mutable QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;

        AlivePeersMap m_alivePeers;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectionsToRemove;

        // alive control
        QElapsedTimer m_aliveSendTimer;
    };
}
#define qnTransactionBus QnTransactionMessageBus::instance()

#endif // __TRANSACTION_MESSAGE_BUS_H_
