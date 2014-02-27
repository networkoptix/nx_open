#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <QUuid>
#include <QByteArray>
#include <QQueue>
#include "utils/network/aio/aioeventhandler.h"
#include "utils/network/http/asynchttpclient.h"

namespace ec2
{

class QnTransactionTransport: public QObject, public aio::AIOEventHandler
{
    Q_OBJECT
public:
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

    QnTransactionTransport(bool isOriginator, bool isClient, QSharedPointer<AbstractStreamSocket> socket = QSharedPointer<AbstractStreamSocket>(), const QUuid& remoteGuid = QUuid());
    ~QnTransactionTransport();

signals:
    void gotTransaction(QByteArray data);
    void stateChanged(State state);
public:
    void doOutgoingConnect(QUrl remoteAddr);
    void addData(const QByteArray& data);
    void close();

    // these getters/setters are using from a single thread
    bool isOriginator() const { return m_originator; }
    bool isClientPeer() const { return m_isClientPeer; }
    QUuid removeGuid() const  { return m_removeGuid; }
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }

    // This is multi thread getters/setters
    bool isReadSync() const;
    void setReadSync(bool value);
    bool isWriteSync() const;
    void setWriteSync(bool value);
    void setState(State state);
    State getState() const;
private:
    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;

    QUuid m_removeGuid;
    bool m_originator;
    bool m_isClientPeer;

    mutable QMutex m_mutex;
    QSharedPointer<AbstractStreamSocket> m_socket;
    nx_http::AsyncHttpClientPtr m_httpClient;
    State m_state;
    std::vector<quint8> m_readBuffer;
    int m_readBufferLen;
    int m_chunkHeaderLen;
    quint32 m_chunkLen;
    int m_sendOffset;
    QQueue<QByteArray> m_dataToSend;
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
