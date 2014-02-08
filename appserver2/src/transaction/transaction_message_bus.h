#ifndef __TRANSACTION_MESSAGE_BUS_H_
#define __TRANSACTION_MESSAGE_BUS_H_

#include <nx_ec/ec_api.h>
#include "nx_ec/data/camera_data.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/common/long_runnable.h"

namespace ec2
{
    class QnTransactionMessageBus;

    class QnTransactionTransport: public QObject, public aio::AIOEventHandler
    {
        Q_OBJECT
    public:

        QnTransactionTransport(QnTransactionMessageBus* owner):
            state(NotDefined), readyForSend(false), readyForRead(false), readBufferLen(0), sendOffset(0), chunkLen(0), isClientSide(false), owner(owner) {}

        enum State {
            NotDefined,
            Connect,
            Connecting,
            ReadyForStreaming,
            ReadChunkHeader,
            ReadChunkBody,
            Closed
        };

        QUrl remoteAddr;
        nx_http::AsyncHttpClientPtr httpClient;
        State state;
        QSharedPointer<AbstractStreamSocket> socket;
        QQueue<QByteArray> dataToSend;
        bool readyForSend;
        bool readyForRead;
        
        std::vector<quint8> readBuffer;
        int readBufferLen;
        int sendOffset;
        quint32 chunkLen;
        bool isClientSide;
        QnTransactionMessageBus* owner;
    public:
        void doClientConnect();
        void startStreaming();
    protected:
        void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw();
        void processError();
    private:
        static void ensureSize(std::vector<quint8>& buffer, int size);
        int chunkHeaderEnd(quint32* size);
    private slots:
        void at_responseReceived( nx_http::AsyncHttpClientPtr );
        void at_httpClientDone(nx_http::AsyncHttpClientPtr);
    };

    class EC2_LIB_API QnTransactionMessageBus: public QnLongRunnable
    {
        Q_OBJECT
    public:
        QnTransactionMessageBus();
        virtual ~QnTransactionMessageBus();

        static QnTransactionMessageBus* instance();
        static void initStaticInstance(QnTransactionMessageBus* instance);

        void addConnectionToPeer(const QUrl& url);
        void removeConnectionFromPeer(const QUrl& url);
        void gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket);
        
        template <class T>
        void setHandler(T* handler) { m_handler = new CustomHandler<T>(handler); }

        template <class T>
        void sendTransaction(const QnTransaction<T>& tran)
        {
            QByteArray buffer;
            OutputBinaryStream<QByteArray> stream(&buffer);
            tran.serialize(&stream);

            QMutexLocker lock(&m_mutex);
            foreach(QnTransactionTransport* transport, m_connections)
                transport->dataToSend.push_back(buffer);
        }
    private:
        friend class QnTransactionTransport;

        class AbstractHandler
        {
        public:
            virtual bool processByteArray(QByteArray& data) = 0;
        };

        template <class T>
        class CustomHandler: public AbstractHandler
        {
        public:
            CustomHandler(T* handler): m_handler(handler) {}

            virtual bool processByteArray(QByteArray& data) override;
        private:
            template <class T2> bool deliveryTransaction(ApiCommand::Value command, InputBinaryStream<QByteArray>& stream);
        private:
            T* m_handler;
        };

        void gotTransaction(QByteArray data)
        {
            m_handler->processByteArray(data);
        }

    protected:
        virtual void run() override;
    private slots:
        void at_timer();
    private:
        AbstractHandler* m_handler;
        QTimer m_timer;
        QMutex m_mutex;

        /*
        class QnTransactionTransport
        {
        public:
            virtual void connect();
            virtual void send();
            virtual void receive();
        };
        */
        
        QVector<QnTransactionTransport*> m_connections;
    };
}

#endif // __TRANSACTION_MESSAGE_BUS_H_
