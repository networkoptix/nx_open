#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <QElapsedTimer>
#include <nx_ec/ec_api.h>
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"
#include "transaction_transport_serializer.h"

namespace ec2
{
    class QnTransactionTransport;

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
        
        template <class T>
        void setHandler(T* handler) { 
            QMutexLocker lock(&m_mutex);
            m_handler = new CustomHandler<T>(handler); 
        }

        void removeHandler() { 
            QMutexLocker lock(&m_mutex);
            m_handler = 0;
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran, const QByteArray& serializedTran)
        {
            QByteArray buffer;
            m_serializer.serialize(buffer, serializedTran);
            sendTransactionInternal(tran, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran)
        {
            QByteArray buffer;
            m_serializer.serialize(buffer, tran);
            sendTransactionInternal(tran, buffer);
        }

        QSet<QnId> alivePeers() const { return m_alivePeers; }
signals:
        void peerLost(QnId);
        void peerFound(QnId);
    private:
        friend class QnTransactionTransport;

        bool isExists(const QnId& removeGuid) const;
        bool isConnecting(const QnId& removeGuid) const;

        class AbstractHandler
        {
        public:
            virtual bool processByteArray(QnTransactionTransport* sender, const QByteArray& data) = 0;
            virtual ~AbstractHandler() {}
        };

        template <class T>
        class CustomHandler: public AbstractHandler
        {
        public:
            CustomHandler(T* handler): m_handler(handler) {}

            virtual bool processByteArray(QnTransactionTransport* sender, const QByteArray& data) override;
        private:
            template <class T2> bool deliveryTransaction(const QnAbstractTransaction&  abstractTran, InputBinaryStream<QByteArray>& stream);
        private:
            T* m_handler;
        };

        typedef QMap<QUuid, QSharedPointer<QnTransactionTransport>> QnConnectionMap;

    private:
        //void gotTransaction(const QnId& remoteGuid, bool isConnectionOriginator, const QByteArray& data);
        void sendTransactionInternal(const QnAbstractTransaction& tran, const QByteArray& chunkData);
        void processConnState(QSharedPointer<QnTransactionTransport> transport);
        bool onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream);
        void onGotTransactionSyncResponse(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream);
        void queueSyncRequest(QSharedPointer<QnTransactionTransport> transport);

        void connectToPeerEstablished(const QnId& id);
        void connectToPeerLost(const QnId& id);
        void sendServerAliveMsg();
        bool isPeerUsing(const QUrl& url);
    private slots:
        void at_timer();
        void at_gotTransaction(QByteArray serializedTran, QSet<QnId> processedPeers);
    private:
        QnTransactionTransportSerializer m_serializer;
        //typedef QMap<QUrl, QSharedPointer<QnTransactionTransport>> RemoveUrlMap;

        //RemoveUrlMap m_remoteUrls;
        QMap<QUrl, bool> m_removeUrls;
        AbstractHandler* m_handler;
        QTimer* m_timer;
        QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;
        QSet<QnId> m_alivePeers;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectionsToRemove;

        // alive control
        QElapsedTimer m_aliveSendTimer;
        QMap<QUuid, qint64> m_lastActivity;
        //QMap<QUuid, qint64> m_peerTimeDiff;
    };
}

#endif // __TRANSACTION_MESSAGE_BUS_H_
