#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <QUuid>
#include <QByteArray>
#include <QQueue>
#include <QSet>
#include "utils/network/aio/aioeventhandler.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/common/id.h"
#include "transaction.h"
#include "transaction_transport_header.h"

namespace ec2
{

class QnTransactionTransport: public QObject, public aio::AIOEventHandler
{
    Q_OBJECT
public:
    enum State {
        NotDefined,
        ConnectingStage1,
        ConnectingStage2,
        Connected,
        NeedStartStreaming,
        ReadyForStreaming,
        Closed,
        Error
    };

    QnTransactionTransport(bool isOriginator, bool isClient, QSharedPointer<AbstractStreamSocket> socket = QSharedPointer<AbstractStreamSocket>(), const QUuid& remoteGuid = QUuid());
    ~QnTransactionTransport();

signals:
    void gotTransaction(QByteArray data, TransactionTransportHeader transportHeader);
    void stateChanged(State state);
public:
    void doOutgoingConnect(QUrl remoteAddr);
    void addData(const QByteArray& data);
    void close();

    // these getters/setters are using from a single thread
    bool isOriginator() const { return m_originator; }
    bool isClientPeer() const { return m_isClientPeer; }
    QUuid remoteGuid() const  { return m_remoteGuid; }
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync() const       { return m_readSync; }
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }
    void setTimeDiff(qint64 diff) { m_timeDiff = diff; }
    qint64 timeDiff() const       { return m_timeDiff; }
    QUrl remoteAddr() const       { return m_remoteAddr; }

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    static bool tryAcquireConnecting(const QnId& remoteGuid, bool isOriginator);
    static bool tryAcquireConnected(const QnId& remoteGuid, bool isOriginator);
    static void connectingCanceled(const QnId& id, bool isOriginator);
    static void connectDone(const QnId& id);
private:
    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;

    QUuid m_remoteGuid;
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
    qint64 m_timeDiff;
    QUrl m_remoteAddr;
    bool m_connected;

    static QSet<QUuid> m_existConn;
    typedef QMap<QUuid, QPair<bool, bool>> ConnectingInfoMap;
    static ConnectingInfoMap m_connectingConn; // first - true if connecting to remove peer in progress, second - true if getting connection from remove peer in progress
    static QMutex m_staticMutex;
private:
    void eventTriggered( AbstractSocket* sock, PollSet::EventType eventType ) throw();
    void closeSocket();
    static void ensureSize(std::vector<quint8>& buffer, std::size_t size);
    int getChunkHeaderEnd(const quint8* data, int dataLen, quint32* const size);
    void processTransactionData( const QByteArray& data);
    void setStateNoLock(State state);
    void cancelConnecting();
    static void connectingCanceledNoLock(const QnId& remoteGuid, bool isOriginator);
private slots:
    void at_responseReceived( nx_http::AsyncHttpClientPtr );
    void at_httpClientDone(nx_http::AsyncHttpClientPtr);
    void repeatDoGet();
};
typedef QSharedPointer<QnTransactionTransport> QnTransactionTransportPtr;
}

Q_DECLARE_METATYPE(ec2::QnTransactionTransport::State);

#endif // __TRANSACTION_TRANSPORT_H__
