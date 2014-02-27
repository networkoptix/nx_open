#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <QUuid>
#include <QByteArray>
#include <QQueue>
#include "utils/network/aio/aioeventhandler.h"
#include "utils/network/http/asynchttpclient.h"

namespace ec2
{

class QnTransactionMessageBus;

class QnTransactionTransport: public QObject, public aio::AIOEventHandler
{
    Q_OBJECT
public:

    QnTransactionTransport(QnTransactionMessageBus* owner):
        state(NotDefined), lastConnectTime(0), readBufferLen(0), 
        chunkHeaderLen(0), sendOffset(0), chunkLen(0), isConnectionOriginator(false), isClientPeer(false), owner(owner),
        readSync(false), writeSync(false) {}
    ~QnTransactionTransport();
    enum State {
        NotDefined,
        Connect,
        Connecting,
        Connected,
        NeedStartStreaming,
        ReadyForStreaming,
        Closed,
        Error
    };

    QSharedPointer<AbstractStreamSocket> socket;
    QUrl remoteAddr;
    QUuid remoteGuid;
    nx_http::AsyncHttpClientPtr httpClient;
    qint64 lastConnectTime;

    bool isConnectionOriginator;
    bool isClientPeer;

    bool readSync;
    bool writeSync;
public:
    void doOutgoingConnect();
    void startStreaming();
    void addData(const QByteArray& data);
    void processError(QSharedPointer<QnTransactionTransport> sibling);
    void sendSyncRequest();
    void setState(State state);
    State getState() const;
private:
    mutable QMutex mutex;
    State state;
    std::vector<quint8> readBuffer;
    int readBufferLen;
    int chunkHeaderLen;
    quint32 chunkLen;
    int sendOffset;
    QnTransactionMessageBus* owner;
    QQueue<QByteArray> dataToSend;
private:
    void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw();
    void closeSocket();
    static void ensureSize(std::vector<quint8>& buffer, int size);
    int getChunkHeaderEnd(const quint8* data, int dataLen, quint32* const size);
    void processTransactionData( const QByteArray& data);
    private slots:
        void at_responseReceived( nx_http::AsyncHttpClientPtr );
        void at_httpClientDone(nx_http::AsyncHttpClientPtr);
};

}

#endif // __TRANSACTION_TRANSPORT_H__
