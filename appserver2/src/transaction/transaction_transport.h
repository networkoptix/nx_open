#ifndef __TRANSACTION_TRANSPORT_H__
#define __TRANSACTION_TRANSPORT_H__

#include <chrono>
#include <deque>

#include <QByteArray>
#include <QElapsedTimer>
#include <QSet>

#include <transaction/transaction.h>
#include <transaction/binary_transaction_serializer.h>
#include <transaction/json_transaction_serializer.h>
#include <transaction/ubjson_transaction_serializer.h>
#include <transaction/transaction_transport_header.h>
#include <transaction/transaction_descriptor.h>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/auth_cache.h>
#include <nx/network/http/httpstreamreader.h>
#include <nx/network/http/http_message_stream_parser.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <utils/common/id.h>
#ifdef _DEBUG
#include <common/common_module.h>
#endif


namespace ec2
{

namespace ConnectionType
{
    enum Type
    {
        none,
        incoming,
        //!this peer is originating one
        outgoing,
        bidirectional
    };

    const char* toString( Type val );
    Type fromString( const QnByteArrayConstRef& str );
}


class QnTransactionTransport
:
    public QObject
{
    Q_OBJECT

public:
    class Locker
    {
    public:
        Locker(QnTransactionTransport* objectToLock)
        :
            m_objectToLock(objectToLock)
        {
            m_objectToLock->lock();
        }
        ~Locker()
        {
            m_objectToLock->unlock();
        }

        void waitForNewTransactionsReady()
        {
            m_objectToLock->waitForNewTransactionsReady();
        }

    private:
        QnTransactionTransport* m_objectToLock;
    };

    static const char* TUNNEL_MULTIPART_BOUNDARY;
    static const char* TUNNEL_CONTENT_TYPE;

    //not using Qt signal/slot because it is undefined in what thread this object lives and in what thread TimerSynchronizationManager lives
    typedef std::function<void(QnTransactionTransport*, const nx_http::HttpHeaders&)> HttpChunkExtensonHandler;
    typedef std::function<void(QnTransactionTransport*, nx_http::HttpHeaders*)> BeforeSendingChunkHandler;

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

    //!Initializer for incoming connection
    QnTransactionTransport(
        const QnUuid& connectionGuid,
        const ApiPeerData& localPeer,
        const ApiPeerData& remotePeer,
        QSharedPointer<AbstractStreamSocket> socket,
        ConnectionType::Type connectionType,
        const nx_http::Request& request,
        const QByteArray& contentEncoding,
        const Qn::UserAccessData &userAccessData);
    //!Initializer for outgoing connection
    QnTransactionTransport( const ApiPeerData& localPeer );
    ~QnTransactionTransport();

    void setBeforeDestroyCallback(std::function<void ()> ttFinishCallback);

    std::chrono::milliseconds connectionKeepAliveTimeout() const;
    int keepAliveProbeCount() const;

signals:
    void gotTransaction(
        Qn::SerializationFormat tranFormat,
        const QByteArray &data,
        const QnTransactionTransportHeader &transportHeader);
    void stateChanged(State state);
    void remotePeerUnauthorized(const QnUuid& id);
    void peerIdDiscovered(const QUrl& url, const QnUuid& id);

private:
    template<class T>
    void sendTransactionImpl(const QnTransaction<T> &transaction, const QnTransactionTransportHeader& _header)
    {
        QnTransactionTransportHeader header(_header);
        NX_ASSERT(header.processedPeers.contains(m_localPeer.id));
        header.fillSequence();
#ifdef _DEBUG

        for (const QnUuid& peer: header.dstPeers) {
            NX_ASSERT(!peer.isNull());
            //NX_ASSERT(peer != qnCommon->moduleGUID());
        }
#endif
        NX_ASSERT(!transaction.isLocal() || m_remotePeer.isClient(), Q_FUNC_INFO, "Invalid transaction type to send!");
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("send transaction %1 to peer %2").arg(transaction.toString()).arg(remotePeer().id.toString()), cl_logDEBUG1 );

        if (m_remotePeer.peerType == Qn::PT_MobileClient && skipTransactionForMobileClient(transaction.command))
            return;

        switch (m_remotePeer.dataFormat) {
        case Qn::JsonFormat:
            if( m_remotePeer.peerType == Qn::PT_MobileClient )
                addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithoutHeader(transaction, header) + QByteArray("\r\n"));
            else
                addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        //case Qn::BnsFormat:
        //    addData(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
        //    break;
        case Qn::UbjsonFormat:
            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        default:
            qWarning() << "Client has requested data in the unsupported format" << m_remotePeer.dataFormat;
            addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(transaction, header));
            break;
        }
    }

public:
    template<class T>
    void sendTransaction(const QnTransaction<T>& transaction, const QnTransactionTransportHeader& header)
    {
        auto remoteAccess = ec2::getTransactionDescriptorByTransaction(transaction)->checkRemotePeerAccessFunc(m_userAccessData, transaction.params);
        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check failed while sending transaction %1 to peer %2")
                   .arg(transaction.toString())
                   .arg(remotePeer().id.toString()),
                cl_logDEBUG1);
            return;
        }
        sendTransactionImpl(transaction, header);
    }

    template<template<typename, typename> class Cont, typename Param, typename A>
    void sendTransaction(const QnTransaction<Cont<Param,A>>& transaction, const QnTransactionTransportHeader& header)
    {
        auto td = ec2::getTransactionDescriptorByTransaction(transaction);
        auto remoteAccess = td->checkRemotePeerAccessFunc(m_userAccessData, transaction.params);

        if (remoteAccess == RemotePeerAccess::Forbidden)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check failed while sending transaction %1 to peer %2")
                   .arg(transaction.toString())
                   .arg(remotePeer().id.toString()),
                cl_logDEBUG1);
            return;
        }
        else if (remoteAccess == RemotePeerAccess::Partial)
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Permission check PARTIALLY failed while sending transaction %1 to peer %2")
                   .arg(transaction.toString())
                   .arg(remotePeer().id.toString()),
                cl_logDEBUG1);

            Cont<Param,A> filteredParams = transaction.params;
            td->filterByReadPermissionFunc(m_userAccessData, filteredParams);
            auto newTransaction = transaction;
            newTransaction.params = filteredParams;

            sendTransactionImpl(newTransaction, header);
        }
        sendTransactionImpl(transaction, header);
    }

    bool sendSerializedTransaction(Qn::SerializationFormat srcFormat, const QByteArray& serializedTran,
                                   const QnTransactionTransportHeader& _header);

    void doOutgoingConnect(const QUrl& remotePeerUrl);
    void close();

    // these getters/setters are using from a single thread
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync(ApiCommand::Value command) const;
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }

    bool isSyncDone() const { return m_syncDone; }
    void setSyncDone(bool value)  {m_syncDone = value;} // end of sync marker received

    bool isSyncInProgress() const { return m_syncInProgress; }
    void setSyncInProgress(bool value)  {m_syncInProgress = value;} // synchronization process in progress

    bool isNeedResync() const { return m_needResync; }
    void setNeedResync(bool value)  {m_needResync = value;} // synchronization process in progress

    QUrl remoteAddr() const;
    SocketAddress remoteSocketAddr() const;

    nx_http::AuthInfoCache::AuthorizationCacheItem authData() const;

    ApiPeerData remotePeer() const { return m_remotePeer; }

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    bool isIncoming() const;

    void setRemoteIdentityTime(qint64 time);
    qint64 remoteIdentityTime() const;

    //!Set \a eventHandler that will receive all http chunk extensions
    /*!
        \return event handler id that may be used to remove event handler with \a QnTransactionTransport::removeEventHandler call
    */
    int setHttpChunkExtensonHandler( HttpChunkExtensonHandler eventHandler );
    //!Set \a eventHandler that will be called before sending each http chunk. It (handler) is allowed to add some extensions to the chunk
    /*!
        \return event handler id that may be used to remove event handler with \a QnTransactionTransport::removeEventHandler call
    */
    int setBeforeSendingChunkHandler( BeforeSendingChunkHandler eventHandler );
    //!Remove event handler, installed by \a QnTransactionTransport::setHttpChunkExtensonHandler or \a QnTransactionTransport::setBeforeSendingChunkHandler
    void removeEventHandler( int eventHandlerID );

    static bool tryAcquireConnecting(const QnUuid& remoteGuid, bool isOriginator);
    static bool tryAcquireConnected(const QnUuid& remoteGuid, bool isOriginator);
    static void connectingCanceled(const QnUuid& id, bool isOriginator);
    static void connectDone(const QnUuid& id);

    void processExtraData();
    void startListening();
    bool isHttpKeepAliveTimeout() const;
    bool hasUnsendData() const;

    void receivedTransaction(
        const nx_http::HttpHeaders& headers,
        const QnByteArrayConstRef& tranData );

    void transactionProcessed();

    QnUuid connectionGuid() const;
    void setIncomingTransactionChannelSocket(
        QSharedPointer<AbstractStreamSocket> socket,
        const nx_http::Request& request,
        const QByteArray& requestBuf );
    //!Transport level logic should use this method to report connection problem
    void connectionFailure();
    Qn::UserAccessData getUserAccessData() const { return m_userAccessData; }

    static bool skipTransactionForMobileClient(ApiCommand::Value command);
    static void fillAuthInfo( const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey );

private:
    struct DataToSend
    {
        QByteArray sourceData;
        QByteArray encodedSourceData;

        DataToSend() {}
        DataToSend( QByteArray&& _sourceData ) : sourceData( std::move(_sourceData) ) {}
    };

    enum PeerRole
    {
        //!peer has established connection
        prOriginating,
        //!peer has accepted connection
        prAccepting
    };

    ApiPeerData m_localPeer;
    ApiPeerData m_remotePeer;

    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;
    bool m_syncDone;
    bool m_syncInProgress; // sync request was send and sync process still in progress
    bool m_needResync; // sync request should be send int the future as soon as possible

    mutable QnMutex m_mutex;
    QSharedPointer<AbstractStreamSocket> m_incomingDataSocket;
    QSharedPointer<AbstractStreamSocket> m_outgoingDataSocket;
    nx_http::AsyncHttpClientPtr m_httpClient;
    State m_state;
    nx::Buffer m_readBuffer;
    //!Holds raw data. It is serialized to http chunk just before sending to socket
    std::deque<DataToSend> m_dataToSend;
    QUrl m_remoteAddr;
    bool m_connected;

    std::map<int, HttpChunkExtensonHandler> m_httpChunkExtensonHandlers;
    std::map<int, BeforeSendingChunkHandler> m_beforeSendingChunkHandlers;
    int m_prevGivenHandlerID;

    static QSet<QnUuid> m_existConn;
    typedef QMap<QnUuid, QPair<bool, bool>> ConnectingInfoMap;
    static ConnectingInfoMap m_connectingConn; // first - true if connecting to remove peer in progress, second - true if getting connection from remove peer in progress
    static QnMutex m_staticMutex;

    QByteArray m_extraData;
    bool m_authByKey;
    QElapsedTimer m_lastReceiveTimer;
    int m_postedTranCount;
    bool m_asyncReadScheduled;
    qint64 m_remoteIdentityTime;
    nx_http::HttpStreamReader m_httpStreamReader;
    std::shared_ptr<nx_http::MultipartContentParser> m_multipartContentParser;
    ConnectionType::Type m_connectionType;
    PeerRole m_peerRole;
    QByteArray m_contentEncoding;
    std::shared_ptr<AbstractByteStreamFilter> m_incomingTransactionStreamParser;
    std::shared_ptr<AbstractByteStreamFilter> m_sizedDecoder;
    bool m_compressResponseMsgBody;
    QnUuid m_connectionGuid;
    nx_http::AsyncHttpClientPtr m_outgoingTranClient;
    bool m_authOutgoingConnectionByServerKey;
    QUrl m_postTranBaseUrl;
    quint64 m_sendKeepAliveTask;
    nx::Buffer m_dummyReadBuffer;
    bool m_base64EncodeOutgoingTransactions;
    std::vector<nx_http::HttpHeader> m_outgoingClientHeaders;
    size_t m_sentTranSequence;
    nx_http::AuthInfoCache::AuthorizationCacheItem m_httpAuthCacheItem;
    //!Number of threads waiting on \a QnTransactionTransport::waitForNewTransactionsReady
    int m_waiterCount;
    QnWaitCondition m_cond;
    std::function<void ()> m_ttFinishCallback;
    std::chrono::milliseconds m_tcpKeepAliveTimeout;
    int m_keepAliveProbeCount;
    std::chrono::milliseconds m_idleConnectionTimeout;
    Qn::UserAccessData m_userAccessData;

private:
    void default_initializer();
    void sendHttpKeepAlive( quint64 taskID );
    //void eventTriggered( AbstractSocket* sock, aio::EventType eventType ) throw();
    void closeSocket();
    void addData(QByteArray data);
    void processTransactionData( const QByteArray& data);
    void setStateNoLock(State state);
    void cancelConnecting();
    static void connectingCanceledNoLock(const QnUuid& remoteGuid, bool isOriginator);
    void addHttpChunkExtensions( nx_http::HttpHeaders* const transactionHeaders );
    void processChunkExtensions( const nx_http::HttpHeaders& httpChunkHeader );
    void onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead );
    void serializeAndSendNextDataBuffer();
    void onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent );
    void setExtraDataBuffer(const QByteArray& data);
    /*!
        \note MUST be called with \a m_mutex locked
    */
    void scheduleAsyncRead();
    bool readCreateIncomingTunnelMessage();
    void receivedTransactionNonSafe( const QnByteArrayConstRef& tranData );
    void startListeningNonSafe();
    void outgoingConnectionEstablished( SystemError::ErrorCode errorCode );
    void startSendKeepAliveTimerNonSafe();
    void monitorConnectionForClosure( SystemError::ErrorCode errorCode, size_t bytesRead );
    QUrl generatePostTranUrl();
    void aggregateOutgoingTransactionsNonSafe();

    /** Destructor will block until unlock is called */
    void lock();
    void unlock();
    //!Blocks till connection is ready to accept new transactions
    void waitForNewTransactionsReady();

private slots:
    void at_responseReceived( const nx_http::AsyncHttpClientPtr& );
    void at_httpClientDone( const nx_http::AsyncHttpClientPtr& );
    void repeatDoGet();
    void postTransactionDone( const nx_http::AsyncHttpClientPtr& );
};

}

Q_DECLARE_METATYPE(ec2::QnTransactionTransport::State);

#endif // __TRANSACTION_TRANSPORT_H__
