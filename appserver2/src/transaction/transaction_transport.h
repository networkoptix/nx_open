#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <QUuid>
#include <QByteArray>
#include <QQueue>
#include <QSet>

#include <transaction/transaction.h>
#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/transaction_transport_header.h>

#include <utils/network/abstract_socket.h>
#include "utils/network/aio/aioeventhandler.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/common/id.h"

#ifdef _DEBUG
#include <common/common_module.h>
#endif


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
    static QString toString( State state );

    QnTransactionTransport(const QnPeerInfo &localPeer,
        const QnPeerInfo &remotePeer = QnPeerInfo(QnId(), QnPeerInfo::Server),
        QSharedPointer<AbstractStreamSocket> socket = QSharedPointer<AbstractStreamSocket>());
    ~QnTransactionTransport();

    static QByteArray encodeHWList(const QList<QByteArray> hwList);
    static QList<QByteArray> decodeHWList(const QByteArray data);

signals:
    void gotTransaction(const QByteArray &data, const QnTransactionTransportHeader &transportHeader);
    void stateChanged(State state);
public:

    template<class T> 
    void sendTransaction(const QnTransaction<T> &transaction, const QnTransactionTransportHeader &header) {
        assert(header.processedPeers.contains(m_localPeer.id));
#ifdef _DEBUG
        foreach (const QnId& peer, header.dstPeers) {
            Q_ASSERT(!peer.isNull());
            Q_ASSERT(peer != qnCommon->moduleGUID());
        }
#endif

        switch (m_remotePeer.peerType) {
        case QnPeerInfo::AndroidClient:
            addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        default:
            addData(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        }
    }

    void doOutgoingConnect(QUrl remoteAddr);
    void close();

    // these getters/setters are using from a single thread
    QList<QByteArray> hwList() const { return m_hwList; }
    void setHwList(const QList<QByteArray>& value) { m_hwList = value; }
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync() const       { return m_readSync; }
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }
    QUrl remoteAddr() const       { return m_remoteAddr; }

    QnPeerInfo remotePeer() const { return m_remotePeer; }

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    static bool tryAcquireConnecting(const QnId& remoteGuid, bool isOriginator);
    static bool tryAcquireConnected(const QnId& remoteGuid, bool isOriginator);
    static void connectingCanceled(const QnId& id, bool isOriginator);
    static void connectDone(const QnId& id);
private:
    QnPeerInfo m_localPeer;
    QnPeerInfo m_remotePeer;

    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;

    QList<QByteArray> m_hwList;

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
    QUrl m_remoteAddr;
    bool m_connected;

    static QSet<QUuid> m_existConn;
    typedef QMap<QUuid, QPair<bool, bool>> ConnectingInfoMap;
    static ConnectingInfoMap m_connectingConn; // first - true if connecting to remove peer in progress, second - true if getting connection from remove peer in progress
    static QMutex m_staticMutex;
private:
    void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw();
    void closeSocket();
    void addData(const QByteArray &data);
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
