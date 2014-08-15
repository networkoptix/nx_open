#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <QUuid>
#include <QByteArray>
#include <QQueue>
#include <QSet>

#include <transaction/transaction.h>
#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/transaction_transport_header.h>

#include <utils/network/abstract_socket.h>
#include "utils/network/aio/aioeventhandler.h"
#include "utils/network/http/asynchttpclient.h"
#include "utils/common/id.h"

#ifdef _DEBUG
#include <common/common_module.h>
#endif

#define TRANSACTION_MESSAGE_BUS_DEBUG

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
        const ApiPeerData &remotePeer = ApiPeerData(QUuid(), Qn::PT_Server),
        QSharedPointer<AbstractStreamSocket> socket = QSharedPointer<AbstractStreamSocket>());
    ~QnTransactionTransport();

signals:
    void gotTransaction(const QByteArray &data, const QnTransactionTransportHeader &transportHeader);
    void stateChanged(State state);
public:

    template<class T> 
    void sendTransaction(const QnTransaction<T> &transaction, const QnTransactionTransportHeader& _header) 
    {
        QnTransactionTransportHeader header(_header);
        assert(header.processedPeers.contains(m_localPeer.id));
        if(header.sequence == 0) 
            header.fillSequence();
#ifdef _DEBUG

        foreach (const QUuid& peer, header.dstPeers) {
            Q_ASSERT(!peer.isNull());
            Q_ASSERT(peer != qnCommon->moduleGUID());
        }
#endif

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        qDebug() << "send transaction to peer " << remotePeer().id << "command=" << ApiCommand::toString(transaction.command) 
                 << "transport seq=" << header.sequence << "db seq=" << transaction.persistentInfo.sequence << "timestamp=" << transaction.persistentInfo.timestamp;
#endif

        switch (m_remotePeer.dataFormat) {
        case Qn::JsonFormat:
            addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        case Qn::BnsFormat:
            addData(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        case Qn::UbjsonFormat:
            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        default:
            qWarning() << "Client has requested data in the unsupported format" << m_remotePeer.dataFormat;
            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        }
    }

    void doOutgoingConnect(QUrl remoteAddr);
    void close();

    // these getters/setters are using from a single thread
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync(ApiCommand::Value command) const;
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }
    QUrl remoteAddr() const       { return m_remoteAddr; }

    ApiPeerData remotePeer() const { return m_remotePeer; }

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    static bool tryAcquireConnecting(const QUuid& remoteGuid, bool isOriginator);
    static bool tryAcquireConnected(const QUuid& remoteGuid, bool isOriginator);
    static void connectingCanceled(const QUuid& id, bool isOriginator);
    static void connectDone(const QUuid& id);
private:
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
    QQueue<QByteArray> m_dataToSend;
    QUrl m_remoteAddr;
    bool m_connected;

    static QSet<QUuid> m_existConn;
    typedef QMap<QUuid, QPair<bool, bool>> ConnectingInfoMap;
    static ConnectingInfoMap m_connectingConn; // first - true if connecting to remove peer in progress, second - true if getting connection from remove peer in progress
    static QMutex m_staticMutex;
    QByteArray m_extraData;
private:
    void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw();
    void closeSocket();
    void addData(const QByteArray &data);
    static void ensureSize(std::vector<quint8>& buffer, std::size_t size);
    int getChunkHeaderEnd(const quint8* data, int dataLen, quint32* const size);
    void processTransactionData( const QByteArray& data);
    void setStateNoLock(State state);
    void cancelConnecting();
    static void connectingCanceledNoLock(const QUuid& remoteGuid, bool isOriginator);
    void setExtraDataBuffer(const QByteArray& data);
private slots:
    void at_responseReceived( nx_http::AsyncHttpClientPtr );
    void at_httpClientDone(nx_http::AsyncHttpClientPtr);
    void repeatDoGet();
};

typedef QSharedPointer<QnTransactionTransport> QnTransactionTransportPtr;
}

Q_DECLARE_METATYPE(ec2::QnTransactionTransport::State);

#endif // __TRANSACTION_TRANSPORT_H__
