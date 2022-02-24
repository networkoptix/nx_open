// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <map>

#include <QByteArray>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QAuthenticator>
#include <QSet>

#include <nx/utils/log/log.h>
#include <nx/utils/uuid.h>
#include <nx/network/abstract_socket.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/auth_cache.h>
#include <nx/network/http/http_stream_reader.h>
#include <nx/network/http/http_message_stream_parser.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "connection_guard.h"
#include "abstract_transaction_transport.h"

namespace ec2
{

struct QnTransactionTransportHeader;

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
    Type fromString( const std::string_view& str );
}


class QnTransactionTransportBase:
    public QnAbstractTransactionTransport,
    public nx::network::aio::BasicPollable
{
    Q_OBJECT

public:
    class Locker
    {
    public:
        Locker(QnTransactionTransportBase* objectToLock)
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
        QnTransactionTransportBase* m_objectToLock;
    };

    static const char* const TUNNEL_MULTIPART_BOUNDARY;
    static const char* const TUNNEL_CONTENT_TYPE;

    //not using Qt signal/slot because it is undefined in what thread this object lives and in what thread TimerSynchronizationManager lives
    typedef std::function<void(QnTransactionTransportBase*, const nx::network::http::HttpHeaders&)> HttpChunkExtensonHandler;
    typedef std::function<void(QnTransactionTransportBase*, nx::network::http::HttpHeaders*)> BeforeSendingChunkHandler;

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
    static const char* toString( State state );

    /** Initializer for incoming connection. */
    QnTransactionTransportBase(
        const QnUuid& localSystemId,
        const std::string& connectionGuid,
        ConnectionLockGuard connectionLockGuard,
        const nx::vms::api::PeerData& localPeer,
        const nx::vms::api::PeerData& remotePeer,
        ConnectionType::Type connectionType,
        const nx::network::http::Request& request,
        const QByteArray& contentEncoding,
        std::chrono::milliseconds tcpKeepAliveTimeout,
        int keepAliveProbeCount,
        nx::network::aio::AbstractAioThread* aioThread = nullptr);

    //!Initializer for outgoing connection
    QnTransactionTransportBase(
        const QnUuid& localSystemId,
        ConnectionGuardSharedState* const connectionGuardSharedState,
        const nx::vms::api::PeerData& localPeer,
        std::chrono::milliseconds tcpKeepAliveTimeout,
        int keepAliveProbeCount,
        nx::network::aio::AbstractAioThread* aioThread = nullptr);

    ~QnTransactionTransportBase();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void stopWhileInAioThread() override;

    /**
     * @param value If true then it is expected that transactionProcessed() is called after
     * processing each transactions received by this object.
     * New transactions receival is suspended while there are >= 16 received transactions pending.
     * true by default.
     */
    void setReceivedTransactionsQueueControlEnabled(bool value);

    void setLocalPeerProtocolVersion(int version);
    void setUserAgent(const std::string& userAgent);

    /** Enables outgoing transaction channel. */
    void setOutgoingConnection(std::unique_ptr<nx::network::AbstractCommunicatingSocket> socket);
    void monitorConnectionForClosure();

    std::chrono::milliseconds connectionKeepAliveTimeout() const;
    int keepAliveProbeCount() const;

    void doOutgoingConnect(const nx::utils::Url &remotePeerUrl);

    // these getters/setters are using from a single thread
    qint64 lastConnectTime() { return m_lastConnectTime; }
    void setLastConnectTime(qint64 value) { m_lastConnectTime = value; }
    bool isReadSync(ApiCommand::Value command) const;
    void setReadSync(bool value)  {m_readSync = value;}
    bool isReadyToSend(ApiCommand::Value command) const;
    void setWriteSync(bool value) { m_writeSync = value; }

    void markAsNotSynchronized();

    bool isSyncDone() const { return m_syncDone; }
    void setSyncDone(bool value)  {m_syncDone = value;} // end of sync marker received

    bool isSyncInProgress() const { return m_syncInProgress; }
    void setSyncInProgress(bool value)  {m_syncInProgress = value;} // synchronization process in progress

    bool isNeedResync() const { return m_needResync; }
    void setNeedResync(bool value)  {m_needResync = value;} // synchronization process in progress

    virtual const nx::vms::api::PeerData& localPeer() const override;
    virtual const nx::vms::api::PeerData& remotePeer() const override;
    virtual nx::utils::Url remoteAddr() const override;
    nx::network::SocketAddress remoteSocketAddr() const;
    int remotePeerProtocolVersion() const;

    virtual std::multimap<QString, QString> httpQueryParams() const override;

    // This is multi thread getters/setters
    void setState(State state);
    State getState() const;

    virtual bool isIncoming() const override;

    void setRemoteIdentityTime(qint64 time);
    qint64 remoteIdentityTime() const;

    //!Set \a eventHandler that will receive all http chunk extensions
    /*!
        \return event handler id that may be used to remove event handler with \a QnTransactionTransportBase::removeEventHandler call
    */
    int setHttpChunkExtensonHandler( HttpChunkExtensonHandler eventHandler );
    //!Set \a eventHandler that will be called before sending each http chunk. It (handler) is allowed to add some extensions to the chunk
    /*!
        \return event handler id that may be used to remove event handler with \a QnTransactionTransportBase::removeEventHandler call
    */
    int setBeforeSendingChunkHandler( BeforeSendingChunkHandler eventHandler );
    //!Remove event handler, installed by \a QnTransactionTransportBase::setHttpChunkExtensonHandler or \a QnTransactionTransportBase::setBeforeSendingChunkHandler
    void removeEventHandler( int eventHandlerID );

    void processExtraData();
    void startListening();
    bool remotePeerSupportsKeepAlive() const;
    bool isHttpKeepAliveTimeout() const;
    bool hasUnsendData() const;

    void receivedTransaction(
        const nx::network::http::HttpHeaders& headers,
        const nx::ConstBufferRefType& tranData );

    void transactionProcessed();

    std::string connectionGuid() const;

    void setIncomingTransactionChannelSocket(
        std::unique_ptr<nx::network::AbstractCommunicatingSocket> socket,
        const nx::network::http::Request& request,
        const QByteArray& requestBuf );

    //!Transport level logic should use this method to report connection problem
    void connectionFailure();

    void setKeepAliveEnabled(bool value);

    void addDataToTheSendQueue(nx::Buffer data);

    void setPostTranUrl(const nx::utils::Url& url);

signals:
    void gotTransaction(
        Qn::SerializationFormat tranFormat,
        QByteArray data,
        const QnTransactionTransportHeader &transportHeader);
    void stateChanged(State state);
    void remotePeerUnauthorized(const QnUuid& id);
    void peerIdDiscovered(const nx::utils::Url& url, const QnUuid& id);
    void onSomeDataReceivedFromRemotePeer();

protected:
    virtual void fillAuthInfo(const nx::network::http::AsyncHttpClientPtr& /*httpClient*/, bool /*authByKey*/) {};

private:
    struct DataToSend
    {
        nx::Buffer sourceData;
        nx::Buffer encodedSourceData;

        DataToSend() {}
        DataToSend(nx::Buffer&& _sourceData ) : sourceData( std::move(_sourceData) ) {}
    };

    enum PeerRole
    {
        //!peer has established connection
        prOriginating,
        //!peer has accepted connection
        prAccepting
    };

    enum class CredentialsSource
    {
        remoteUrl,
        serverKey,
        none,
    };

    QnUuid m_localSystemId;
    const nx::vms::api::PeerData m_localPeer;
    nx::vms::api::PeerData m_remotePeer;

    qint64 m_lastConnectTime;

    bool m_readSync;
    bool m_writeSync;
    bool m_syncDone;
    bool m_syncInProgress; // sync request was send and sync process still in progress
    bool m_needResync; // sync request should be send int the future as soon as possible

    mutable nx::Mutex m_mutex;
    std::unique_ptr<nx::network::AbstractCommunicatingSocket> m_incomingDataSocket;
    std::unique_ptr<nx::network::AbstractCommunicatingSocket> m_outgoingDataSocket;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    State m_state;
    nx::Buffer m_readBuffer;
    //!Holds raw data. It is serialized to http chunk just before sending to socket
    std::deque<DataToSend> m_dataToSend;
    nx::utils::Url m_remoteAddr;
    bool m_connected;

    std::map<int, HttpChunkExtensonHandler> m_httpChunkExtensonHandlers;
    std::map<int, BeforeSendingChunkHandler> m_beforeSendingChunkHandlers;
    int m_prevGivenHandlerID;

    nx::Buffer m_extraData;
    CredentialsSource m_credentialsSource;
    QElapsedTimer m_lastReceiveTimer;
    int m_postedTranCount = 0;
    bool m_asyncReadScheduled;
    qint64 m_remoteIdentityTime;
    nx::network::http::HttpStreamReader m_httpStreamReader;
    std::shared_ptr<nx::network::http::MultipartContentParser> m_multipartContentParser;
    ConnectionType::Type m_connectionType;
    const PeerRole m_peerRole;
    QByteArray m_contentEncoding;
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_incomingTransactionStreamParser;
    std::shared_ptr<nx::utils::bstream::AbstractByteStreamFilter> m_sizedDecoder;
    bool m_compressResponseMsgBody;
    std::string m_connectionGuid;
    ConnectionGuardSharedState* const m_connectionGuardSharedState;
    std::unique_ptr<ConnectionLockGuard> m_connectionLockGuard;
    nx::network::http::AsyncHttpClientPtr m_outgoingTranClient;
    bool m_authOutgoingConnectionByServerKey;
    nx::utils::Url m_postTranBaseUrl;
    std::optional<nx::utils::Url> m_mockedUpPostTranUrl;
    nx::Buffer m_dummyReadBuffer;
    bool m_base64EncodeOutgoingTransactions;
    std::vector<nx::network::http::HttpHeader> m_outgoingClientHeaders;
    size_t m_sentTranSequence;
    //!Number of threads waiting on \a QnTransactionTransportBase::waitForNewTransactionsReady
    int m_waiterCount;
    nx::WaitCondition m_cond;
    std::chrono::milliseconds m_tcpKeepAliveTimeout;
    int m_keepAliveProbeCount;
    std::chrono::milliseconds m_idleConnectionTimeout;
    QAuthenticator m_remotePeerCredentials;
    std::unique_ptr<nx::network::aio::Timer> m_timer;
    bool m_remotePeerSupportsKeepAlive;
    bool m_isKeepAliveEnabled;
    int m_remotePeerEcProtoVersion;
    bool m_receivedTransactionsQueueControlEnabled = true;
    int m_localPeerProtocolVersion;
    std::optional<std::string> m_userAgent;
    std::multimap<QString, QString> m_httpQueryParams;

private:
    QnTransactionTransportBase(
        const QnUuid& localSystemId,
        ConnectionGuardSharedState* const connectionGuardSharedState,
        const nx::vms::api::PeerData& localPeer,
        PeerRole peerRole,
        std::chrono::milliseconds tcpKeepAliveTimeout,
        int keepAliveProbeCount,
        nx::network::aio::AbstractAioThread* aioThread);

    void sendHttpKeepAlive();
    void processTransactionData( const nx::Buffer& data);
    void setStateNoLock(State state);
    void cancelConnecting();
    static void connectingCanceledNoLock(const QnUuid& remoteGuid, bool isOriginator);
    void addHttpChunkExtensions( nx::network::http::HttpHeaders* const transactionHeaders );
    void processChunkExtensions( const nx::network::http::HttpHeaders& httpChunkHeader );
    void onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead );
    void serializeAndSendNextDataBuffer();
    void onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent );
    void setExtraDataBuffer(nx::Buffer data);
    /*!
        \note MUST be called with \a m_mutex locked
    */
    void scheduleAsyncRead();
    bool readCreateIncomingTunnelMessage();
    void receivedTransactionNonSafe( const nx::ConstBufferRefType& tranData );
    void startListeningNonSafe();
    void outgoingConnectionEstablished( SystemError::ErrorCode errorCode );
    void startSendKeepAliveTimerNonSafe();
    void onMonitorConnectionForClosure( SystemError::ErrorCode errorCode, size_t bytesRead );
    nx::utils::Url generatePostTranUrl();
    void aggregateOutgoingTransactionsNonSafe();

    /** Destructor will block until unlock is called */
    void lock();
    void unlock();
    //!Blocks till connection is ready to accept new transactions
    void waitForNewTransactionsReady();

private slots:
    void at_responseReceived( const nx::network::http::AsyncHttpClientPtr& );
    void at_httpClientDone( const nx::network::http::AsyncHttpClientPtr& );
    void repeatDoGet();
    void postTransactionDone( const nx::network::http::AsyncHttpClientPtr& );
};

}

Q_DECLARE_METATYPE(ec2::QnTransactionTransportBase::State);
