#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <nx_ec/ec_api.h>
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"

namespace ec2
{
    class QnTransactionMessageBus;
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
        void gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QUrlQuery& params);
        
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
            serializeTransaction(buffer, serializedTran);
            sendTransactionInternal(!tran.originGuid.isNull() ? tran.originGuid : tran.id.peerGUID, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran)
        {
            QByteArray buffer;
            serializeTransaction(buffer, tran);
            sendTransactionInternal(!tran.originGuid.isNull() ? tran.originGuid : tran.id.peerGUID, buffer);
        }

        static void serializeTransaction(QByteArray& buffer, const QByteArray& serializedTran)
        {
            buffer.reserve(serializedTran.size() + 12);
            buffer.append("00000000\r\n");
            buffer.append(serializedTran);
            buffer.append("\r\n"); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        template <class T>
        static void serializeTransaction(QByteArray& buffer, const QnTransaction<T>& tran) 
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            stream.write("00000000\r\n",10);
            tran.serialize(&stream);
            stream.write("\r\n",2); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

    signals:
        void sendGotTransaction(QnId remoteGuid, bool isOriginator, QByteArray data);
    private:
        friend class QnTransactionTransport;

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

        struct ConnectionsToPeer 
        {
            ConnectionsToPeer() {}
            void proxyIncomingTransaction(const QnAbstractTransaction& tran, const QByteArray& data);
            void sendOutgoingTran(const QByteArray& data);

            QSharedPointer<QnTransactionTransport> incomeConn;
            QSharedPointer<QnTransactionTransport> outcomeConn;
        };
        typedef QMap<QUuid, ConnectionsToPeer> QnConnectionMap;

    private:
        void gotTransaction(const QnId& remoteGuid, bool isConnectionOriginator, const QByteArray& data);
        void sendTransactionInternal(const QnId& originGuid, const QByteArray& chunkData);
        void processConnState(QSharedPointer<QnTransactionTransport> &transport);
        void sendSyncRequestIfRequired(QSharedPointer<QnTransactionTransport> transport);
        QSharedPointer<QnTransactionTransport> getSibling(QSharedPointer<QnTransactionTransport> transport);
        static bool onGotTransactionSyncRequest(QSharedPointer<QnTransactionTransport> sender, InputBinaryStream<QByteArray>& stream);
        static void onGotTransactionSyncResponse(QSharedPointer<QnTransactionTransport> sender);
        static void toFormattedHex(quint8* dst, quint32 payloadSize);
    private slots:
        void at_timer();
        void at_gotTransaction(QnId remoteGuid, bool isOriginator, QByteArray serializedTran);
    private:
        AbstractHandler* m_handler;
        QTimer* m_timer;
        QMutex m_mutex;
        QThread *m_thread;
        QnConnectionMap m_connections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectionsToRemove;
    };
}

#endif // __TRANSACTION_MESSAGE_BUS_H_
