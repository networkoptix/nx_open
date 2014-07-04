#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <deque>

#include <QUuid>
#include <QByteArray>
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

    QnTransactionTransport(const ApiPeerData &localPeer,
        const ApiPeerData &remotePeer = ApiPeerData(QnId(), Qn::PT_Server),
        QSharedPointer<AbstractStreamSocket> socket = QSharedPointer<AbstractStreamSocket>());
    ~QnTransactionTransport();

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
        case Qn::PT_AndroidClient:
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
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync() const       { return m_readSync; }
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }
    QUrl remoteAddr() const       { return m_remoteAddr; }

    ApiPeerData remotePeer() const { return m_remotePeer; }

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    static bool tryAcquireConnecting(const QnId& remoteGuid, bool isOriginator);
    static bool tryAcquireConnected(const QnId& remoteGuid, bool isOriginator);
    static void connectingCanceled(const QnId& id, bool isOriginator);
    static void connectDone(const QnId& id);

private:
    struct DataToSend
    {
        QByteArray sourceData;
        QByteArray encodedSourceData;

        DataToSend() {}
        DataToSend( QByteArray&& _sourceData ) : sourceData( std::move(_sourceData) ) {}
    };

    ApiPeerData m_localPeer;
    ApiPeerData m_remotePeer;

    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;

    mutable QMutex m_mutex;
    QSharedPointer<AbstractStreamSocket> m_socket;
    nx_http::AsyncHttpClientPtr m_httpClient;
    State m_state;
    std::vector<quint8> m_readBuffer;
    int m_readBufferLen;
    int m_chunkHeaderLen;
    quint32 m_chunkLen;
    int m_sendOffset;
    //!Holds raw data. It is serialized to http chunk just before sending to socket
    std::deque<DataToSend> m_dataToSend;
    QUrl m_remoteAddr;
    bool m_connected;

    static QSet<QUuid> m_existConn;
    typedef QMap<QUuid, QPair<bool, bool>> ConnectingInfoMap;
    static ConnectingInfoMap m_connectingConn; // first - true if connecting to remove peer in progress, second - true if getting connection from remove peer in progress
    static QMutex m_staticMutex;
private:
    void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw();
    void closeSocket();
    void addData(QByteArray &&data);
    /*!
        \return in case of success returns number of bytes read from \a data. In case of parse error returns 0
        \note In case of error \a chunkHeader contents are undefined
    */
    int readChunkHeader(const quint8* data, int dataLen, nx_http::ChunkHeader* const chunkHeader);
    void processTransactionData( const QByteArray& data);
    void setStateNoLock(State state);
    void cancelConnecting();
    static void connectingCanceledNoLock(const QnId& remoteGuid, bool isOriginator);
    void addHttpChunkExtensions( std::vector<nx_http::ChunkExtension>* const chunkExtensions );
    void processChunkExtensions( const nx_http::ChunkHeader& httpChunkHeader );
private slots:
    void at_responseReceived( const nx_http::AsyncHttpClientPtr& );
    void at_httpClientDone( const nx_http::AsyncHttpClientPtr& );
    void repeatDoGet();
};

typedef QSharedPointer<QnTransactionTransport> QnTransactionTransportPtr;
}

Q_DECLARE_METATYPE(ec2::QnTransactionTransport::State);

#endif // __TRANSACTION_TRANSPORT_H__
