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

    class QnTransactionTransport: public QObject, public aio::AIOEventHandler
    {
        Q_OBJECT
    public:

        QnTransactionTransport(QnTransactionMessageBus* owner):
            state(NotDefined), lastConnectTime(0), readyForSend(false), readyForRead(false), readBufferLen(0), chunkHeaderLen(0), sendOffset(0), chunkLen(0), isConnectionOriginator(false), isClientPeer(false), owner(owner) {}
        ~QnTransactionTransport();
        enum State {
            NotDefined,
            Connect,
            Connecting,
            WaitForTranSync, // ignore incoming data until we got initial sync transaction
            ReadyForStreaming,
            Closed
        };

        QUrl remoteAddr;
        QUuid remoteGuid;
        nx_http::AsyncHttpClientPtr httpClient;
        State state;
        qint64 lastConnectTime;
        QSharedPointer<AbstractStreamSocket> socket;
        QQueue<QByteArray> dataToSend;
        
        bool readyForSend;
        bool readyForRead;
        
        std::vector<quint8> readBuffer;
        int readBufferLen;
        int chunkHeaderLen;
        quint32 chunkLen;
        int sendOffset;
        bool isConnectionOriginator;
        bool isClientPeer;
        QnTransactionMessageBus* owner;
        QMutex mutex;
    public:
        void doClientConnect();
        void startStreaming();
        void addData(const QByteArray& data);
    protected:
        void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw();
        void closeSocket();
        void processError();
    private:
        static void ensureSize(std::vector<quint8>& buffer, int size);
        int getChunkHeaderEnd(const quint8* data, int dataLen, quint32* const size);
        void processTransactionData( const QByteArray& data);
    private slots:
        void at_responseReceived( nx_http::AsyncHttpClientPtr );
        void at_httpClientDone(nx_http::AsyncHttpClientPtr);
    };

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
            buffer.reserve(serializedTran.size() + 12);
            buffer.append("00000000\r\n");
            buffer.append(serializedTran);
            buffer.append("\r\n"); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
            sendTransactionInternal(!tran.originGuid.isNull() ? tran.originGuid : tran.id.peerGUID, buffer);
        }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran)
        {
            QByteArray buffer;
            serializeTransaction(buffer, tran);
            sendTransactionInternal(!tran.originGuid.isNull() ? tran.originGuid : tran.id.peerGUID, buffer);
        }

    private:
        friend class QnTransactionTransport;

        void toFormattedHex(quint8* dst, quint32 payloadSize);

        template <class T>
        void serializeTransaction(QByteArray& buffer, const QnTransaction<T>& tran) 
        {
            OutputBinaryStream<QByteArray> stream(&buffer);
            buffer.append("00000000\r\n");
            tran.serialize(&stream);
            buffer.append("\r\n"); // chunk end
            quint32 payloadSize = buffer.size() - 12;
            toFormattedHex((quint8*) buffer.data() + 7, payloadSize);
        }

        void sendTransactionInternal(const QnId& originGuid, const QByteArray& buffer);

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

        inline void gotTransaction(QnTransactionTransport* sender, QByteArray data) { emit sendGotTransaction(sender, data); }
    signals:
        void sendGotTransaction(QnTransactionTransport* sender, QByteArray data);
    private slots:
        void at_timer();
        void at_gotTransaction(QnTransactionTransport* sender, QByteArray data);
    private:
        void processConnState(QSharedPointer<QnTransactionTransport> &transport);
        void sendSyncRequestIfRequired(QnTransactionTransport* transport);
        static bool onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream);
        void moveOutgoingConnToMainList(QnTransactionTransport* transport);
    private:
        AbstractHandler* m_handler;
        QTimer* m_timer;
        QMutex m_mutex;
        QThread *m_thread;

        //QVector<QnTransactionTransport*> m_connections;

        struct QnConnectionsPair {
            QnConnectionsPair() {}
            void proxyIncomingTransaction(const QnAbstractTransaction& tran, const QByteArray& data);
            void sendOutgoingTran(const QByteArray& data);

            QSharedPointer<QnTransactionTransport> incomeConn;
            QSharedPointer<QnTransactionTransport> outcomeConn;
        };

        typedef QMap<QUuid, QnConnectionsPair> QnConnectionMap;
        QnConnectionMap m_connections;
        QVector<QSharedPointer<QnTransactionTransport>> m_connectingConnections;
    };
}

#endif // __TRANSACTION_MESSAGE_BUS_H_
