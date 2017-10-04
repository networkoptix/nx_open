
#include "transaction_transport_base.h"

#include <atomic>

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>

#include <nx/network/http/base64_decoder_filter.h>
#include <nx/network/socket_factory.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/byte_stream/sized_data_decoder.h>
#include <nx/utils/gzip/gzip_compressor.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <nx/utils/system_error.h>

#include <nx/cloud/cdb/api/ec2_request_paths.h>
#include <nx_ec/ec_proto_version.h>
#include <nx/utils/byte_stream/custom_output_stream.h>
#include <utils/common/util.h>
#include <nx/network/http/custom_headers.h>
#include <api/global_settings.h>
#include <common/common_module.h>

//#define USE_SINGLE_TWO_WAY_CONNECTION
//!if not defined, ubjson is used
//#define USE_JSON
#ifndef USE_JSON
#define ENCODE_TO_BASE64
#endif
#define AGGREGATE_TRANSACTIONS_BEFORE_SEND

namespace {

/*!
    Real number of transactions posted to the QnTransactionMessageBus can be greater
    (all transactions that read with single read from socket)
*/
static const int MAX_TRANS_TO_POST_AT_A_TIME = 16;

} // namespace

namespace ec2
{

namespace ConnectionType
{
    const char* toString( Type val )
    {
        switch( val )
        {
            case incoming:
                return "incoming";
            case outgoing:
                return "outgoing";
            case bidirectional:
                return "bidirectional";
            default:
                return "none";
        }
    }

    Type fromString( const QnByteArrayConstRef& str )
    {
        if( str == "incoming" )
            return incoming;
        else if( str == "outgoing" )
            return outgoing;
        else if( str == "bidirectional" )
            return bidirectional;
        else
            return none;
    }
}


static const int DEFAULT_READ_BUFFER_SIZE = 4 * 1024;
static const int SOCKET_TIMEOUT = 1000 * 1000;
//following value is for VERY slow networks and VERY large transactions (e.g., some large image)
    //Connection keep-alive timeout is not influenced by this value
static const std::chrono::minutes kSocketSendTimeout(23);
const char* const QnTransactionTransportBase::TUNNEL_MULTIPART_BOUNDARY = "ec2boundary";
const char* const QnTransactionTransportBase::TUNNEL_CONTENT_TYPE = "multipart/mixed; boundary=ec2boundary";

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    ConnectionGuardSharedState* const connectionGuardSharedState,
    const ApiPeerData& localPeer,
    PeerRole peerRole,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount)
:
    m_localSystemId(localSystemId),
    m_localPeer(localPeer),
    m_peerRole(peerRole),
    m_connectionGuardSharedState(connectionGuardSharedState),
    m_tcpKeepAliveTimeout(tcpKeepAliveTimeout),
    m_keepAliveProbeCount(keepAliveProbeCount),
    m_idleConnectionTimeout(tcpKeepAliveTimeout * keepAliveProbeCount),
    m_timer(std::make_unique<nx::network::aio::Timer>()),
    m_remotePeerEcProtoVersion(nx_ec::INITIAL_EC2_PROTO_VERSION),
    m_localPeerProtocolVersion(nx_ec::EC2_PROTO_VERSION)
{
    m_timer->bindToAioThread(getAioThread());

    m_lastConnectTime = 0;
    m_readSync = false;
    m_writeSync = false;
    m_syncDone = false;
    m_syncInProgress = false;
    m_needResync = false;
    m_state = NotDefined;
    m_connected = false;
    m_prevGivenHandlerID = 0;
    m_credentialsSource = CredentialsSource::serverKey;
    m_postedTranCount = 0;
    m_asyncReadScheduled = false;
    m_remoteIdentityTime = 0;
    m_connectionType = ConnectionType::none;
    m_compressResponseMsgBody = false;
    m_authOutgoingConnectionByServerKey = true;
    m_base64EncodeOutgoingTransactions = false;
    m_sentTranSequence = 0;
    m_waiterCount = 0;
    m_remotePeerSupportsKeepAlive = false;
    m_isKeepAliveEnabled = true;
}

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    const QnUuid& connectionGuid,
    ConnectionLockGuard connectionLockGuard,
    const ApiPeerData& localPeer,
    const ApiPeerData& remotePeer,
    ConnectionType::Type connectionType,
    const nx_http::Request& request,
    const QByteArray& contentEncoding,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount)
:
    QnTransactionTransportBase(
        localSystemId,
        nullptr,
        localPeer,
        prAccepting,
        tcpKeepAliveTimeout,
        keepAliveProbeCount)
{
    m_remotePeer = remotePeer;
    m_connectionType = connectionType;
    m_contentEncoding = contentEncoding;
    m_connectionGuid = connectionGuid;
    m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
        std::move(connectionLockGuard));

    nx_http::HttpHeaders::const_iterator ec2ProtoVersionIter =
        request.headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);

    m_remotePeerEcProtoVersion =
        ec2ProtoVersionIter == request.headers.end()
        ? nx_ec::INITIAL_EC2_PROTO_VERSION
        : ec2ProtoVersionIter->second.toInt();

    //TODO #ak use binary filter stream for serializing transactions
    m_base64EncodeOutgoingTransactions = nx_http::getHeaderValue(
        request.headers, Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true";

    auto keepAliveHeaderIter = request.headers.find(Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME);
    if (keepAliveHeaderIter != request.headers.end())
    {
        m_remotePeerSupportsKeepAlive = true;
        nx_http::header::KeepAlive keepAliveHeader;
        if (keepAliveHeader.parse(keepAliveHeaderIter->second))
            m_tcpKeepAliveTimeout = std::max(
                std::chrono::duration_cast<std::chrono::seconds>(m_tcpKeepAliveTimeout),
                keepAliveHeader.timeout);
    }

    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    using namespace std::placeholders;
    if( m_contentEncoding == "gzip" )
        m_compressResponseMsgBody = true;

    //creating parser sequence: http_msg_stream_parser -> ext_headers_processor -> transaction handler
    auto incomingTransactionsRequestsParser = std::make_shared<nx_http::HttpMessageStreamParser>();
    std::weak_ptr<nx_http::HttpMessageStreamParser> incomingTransactionsRequestsParserWeak(
        incomingTransactionsRequestsParser );

    auto extensionHeadersProcessor = nx::utils::bstream::makeFilterWithFunc( //this filter receives single HTTP message
        [this, incomingTransactionsRequestsParserWeak]() {
            if( auto incomingTransactionsRequestsParserStrong = incomingTransactionsRequestsParserWeak.lock() )
                processChunkExtensions( incomingTransactionsRequestsParserStrong->currentMessage().headers() );
        } );

    extensionHeadersProcessor->setNextFilter( nx::utils::bstream::makeCustomOutputStream(
        std::bind(
            &QnTransactionTransportBase::receivedTransactionNonSafe,
            this,
            std::placeholders::_1 ) ) );

    incomingTransactionsRequestsParser->setNextFilter( std::move(extensionHeadersProcessor) );

    m_incomingTransactionStreamParser = std::move(incomingTransactionsRequestsParser);
}

QnTransactionTransportBase::QnTransactionTransportBase(
    const QnUuid& localSystemId,
    ConnectionGuardSharedState* const connectionGuardSharedState,
    const ApiPeerData& localPeer,
    std::chrono::milliseconds tcpKeepAliveTimeout,
    int keepAliveProbeCount)
:
    QnTransactionTransportBase(
        localSystemId,
        connectionGuardSharedState,
        localPeer,
        prOriginating,
        tcpKeepAliveTimeout,
        keepAliveProbeCount)
{
    m_connectionType =
#ifdef USE_SINGLE_TWO_WAY_CONNECTION
        ConnectionType::bidirectional
#else
        ConnectionType::incoming
#endif
        ;
    m_connectionGuid = QnUuid::createUuid();
#ifdef ENCODE_TO_BASE64
    m_base64EncodeOutgoingTransactions = true;
#endif

    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();

    NX_LOGX(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG2);

    //creating parser sequence: multipart_parser -> ext_headers_processor -> transaction handler
    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    std::weak_ptr<nx_http::MultipartContentParser> multipartContentParserWeak( m_multipartContentParser );
    auto extensionHeadersProcessor = nx::utils::bstream::makeFilterWithFunc( //this filter receives single multipart message
        [this, multipartContentParserWeak]() {
            if( auto multipartContentParser = multipartContentParserWeak.lock() )
                processChunkExtensions( multipartContentParser->prevFrameHeaders() );
        } );
    extensionHeadersProcessor->setNextFilter( nx::utils::bstream::makeCustomOutputStream(
        std::bind(
            &QnTransactionTransportBase::receivedTransactionNonSafe,
            this,
            std::placeholders::_1 ) ) );
    m_multipartContentParser->setNextFilter( std::move(extensionHeadersProcessor) );

    m_incomingTransactionStreamParser = m_multipartContentParser;
}

QnTransactionTransportBase::~QnTransactionTransportBase()
{
    NX_LOGX(QnLog::EC2_TRAN_LOG, lit("~QnTransactionTransportBase for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG2);

    stopWhileInAioThread();

    // TODO: #ak: move following logic to ::ec2::QnTransactionTransport
    // since it can result in deadlock if used in aio thread.
    // Though, currently cloud_db does not use following logic.
    {
        QnMutexLocker lk( &m_mutex );
        m_state = Closed;
        m_cond.wakeAll();   //signalling waiters that connection is being closed
        //waiting for waiters to quit
        while( m_waiterCount > 0 )
            m_cond.wait( lk.mutex() );
    }
}

void QnTransactionTransportBase::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    nx::network::aio::BasicPollable::bindToAioThread(aioThread);

    m_timer->bindToAioThread(aioThread);
    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
    if (m_outgoingTranClient)
        m_outgoingTranClient->bindToAioThread(aioThread);
    if (m_outgoingDataSocket)
        m_outgoingDataSocket->bindToAioThread(aioThread);
    if (m_incomingDataSocket)
        m_incomingDataSocket->bindToAioThread(aioThread);
}

void QnTransactionTransportBase::stopWhileInAioThread()
{
    m_timer.reset();
    m_httpClient.reset();
    m_outgoingTranClient.reset();
    m_outgoingDataSocket.reset();
    m_incomingDataSocket.reset();
}

void QnTransactionTransportBase::setLocalPeerProtocolVersion(int version)
{
    m_localPeerProtocolVersion = version;
}

void QnTransactionTransportBase::setOutgoingConnection(
    QSharedPointer<AbstractCommunicatingSocket> socket)
{
    using namespace std::chrono;

    m_outgoingDataSocket = std::move(socket);
    m_outgoingDataSocket->bindToAioThread(getAioThread());
    if (!m_outgoingDataSocket->setSendTimeout(
            duration_cast<milliseconds>(kSocketSendTimeout).count()))
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lit("Error setting socket write timeout for transaction connection %1 received from %2")
                .arg(m_connectionGuid.toString())
                .arg(m_outgoingDataSocket->getForeignAddress().toString())
                .arg(SystemError::toString(osErrorCode)),
            cl_logWARNING);
    }

    if (m_connectionType == ConnectionType::bidirectional)
        m_incomingDataSocket = m_outgoingDataSocket;
}

void QnTransactionTransportBase::monitorConnectionForClosure()
{
    startSendKeepAliveTimerNonSafe();

    m_dummyReadBuffer.reserve(DEFAULT_READ_BUFFER_SIZE);
    if (!m_outgoingDataSocket->setNonBlockingMode(true))
    {
        return m_outgoingDataSocket->post(
            [this, error = SystemError::getLastOSErrorCode()]()
        {
            onMonitorConnectionForClosure(error, (size_t) -1);
        });
    }

    m_outgoingDataSocket->readSomeAsync(
        &m_dummyReadBuffer,
        [this](SystemError::ErrorCode c, size_t s){ onMonitorConnectionForClosure(c, s); });
}

std::chrono::milliseconds QnTransactionTransportBase::connectionKeepAliveTimeout() const
{
    return m_tcpKeepAliveTimeout;
}

int QnTransactionTransportBase::keepAliveProbeCount() const
{
    return m_keepAliveProbeCount;
}

void QnTransactionTransportBase::addDataToTheSendQueue(QByteArray data)
{
    QnMutexLocker lock( &m_mutex );
    if( m_base64EncodeOutgoingTransactions )
    {
        //adding size before transaction data
        const uint32_t dataSize = htonl(data.size());
        QByteArray dataWithSize;
        dataWithSize.resize( sizeof(dataSize) + data.size() );
        //TODO #ak too many memcopy here. Should use stream base64 encoder to write directly to the output buffer
        memcpy( dataWithSize.data(), &dataSize, sizeof(dataSize) );
        memcpy(
            dataWithSize.data()+sizeof(dataSize),
            data.constData(),
            data.size() );
        data.clear();
        m_dataToSend.push_back( std::move(dataWithSize) );
    }
    else
    {
        m_dataToSend.push_back( std::move( data ) );
    }
    if( m_dataToSend.size() == 1 )
        serializeAndSendNextDataBuffer();
}

void QnTransactionTransportBase::setState(State state)
{
    QnMutexLocker lock( &m_mutex );
    setStateNoLock(state);
}

void QnTransactionTransportBase::processExtraData()
{
    QnMutexLocker lock( &m_mutex );
    if( !m_extraData.isEmpty() )
    {
        processTransactionData(m_extraData);
        m_extraData.clear();
    }
}

void QnTransactionTransportBase::startListening()
{
    QnMutexLocker lock( &m_mutex );
    startListeningNonSafe();
}

void QnTransactionTransportBase::setStateNoLock(State state)
{
    if (state == Connected)
        m_connected = true;

    if (m_state == Error && state != Closed)
    {
        ; // only Error -> Closed setState is allowed
    }
    else if (m_state == Closed)
    {
        ; // no state allowed after Closed
    }
    else if (this->m_state != state)
    {
        this->m_state = state;

        // TODO: #ak: if stateChanged handler frees this object then m_mutex is destroyed locked
        nx::utils::ObjectDestructionFlag::Watcher watcher(
            &m_connectionFreedFlag);
        emit stateChanged(state);
        if (watcher.objectDestroyed())
            return; //connection has been removed by handler
    }
    m_cond.wakeAll();
}

const ec2::ApiPeerData& QnTransactionTransportBase::localPeer() const
{
    return m_localPeer;
}

const ec2::ApiPeerData& QnTransactionTransportBase::remotePeer() const
{
    return m_remotePeer;
}

nx::utils::Url QnTransactionTransportBase::remoteAddr() const
{
    QnMutexLocker lock(&m_mutex);
    // Emulating deep copy here
    nx::utils::Url tmpUrl(m_remoteAddr);
    tmpUrl.setUserName(tmpUrl.userName());
    return tmpUrl;
}

SocketAddress QnTransactionTransportBase::remoteSocketAddr() const
{
    QnMutexLocker lock(&m_mutex);
    SocketAddress addr = SocketAddress(
        m_remoteAddr.host(),
        m_remoteAddr.port()
    );

    return addr;
}

int QnTransactionTransportBase::remotePeerProtocolVersion() const
{
    return m_remotePeerEcProtoVersion;
}

nx_http::AuthInfoCache::AuthorizationCacheItem QnTransactionTransportBase::authData() const
{
    QnMutexLocker lock( &m_mutex );
    return m_httpAuthCacheItem;
}

QnTransactionTransportBase::State QnTransactionTransportBase::getState() const
{
    QnMutexLocker lock( &m_mutex );
    return m_state;
}

bool QnTransactionTransportBase::isIncoming() const {
    QnMutexLocker lock(&m_mutex);
    return m_peerRole == prAccepting;
}

int QnTransactionTransportBase::setHttpChunkExtensonHandler( HttpChunkExtensonHandler eventHandler )
{
    QnMutexLocker lk( &m_mutex );
    m_httpChunkExtensonHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

int QnTransactionTransportBase::setBeforeSendingChunkHandler( BeforeSendingChunkHandler eventHandler )
{
    QnMutexLocker lk( &m_mutex );
    m_beforeSendingChunkHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

void QnTransactionTransportBase::removeEventHandler( int eventHandlerID )
{
    QnMutexLocker lk( &m_mutex );
    m_httpChunkExtensonHandlers.erase( eventHandlerID );
    m_beforeSendingChunkHandlers.erase( eventHandlerID );
}

void QnTransactionTransportBase::doOutgoingConnect(const nx::utils::Url& remotePeerUrl)
{
    NX_LOG( QnLog::EC2_TRAN_LOG,
        lm("QnTransactionTransportBase::doOutgoingConnect. remotePeerUrl = %1").arg(remotePeerUrl),
        cl_logDEBUG2 );

    setState(ConnectingStage1);

    m_httpClient = nx_http::AsyncHttpClient::create();
    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->setSendTimeoutMs(m_idleConnectionTimeout.count());
    m_httpClient->setResponseReadTimeoutMs(m_idleConnectionTimeout.count());
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &QnTransactionTransportBase::at_responseReceived,
        Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnTransactionTransportBase::at_httpClientDone,
        Qt::DirectConnection);

    if (remotePeerUrl.userName().isEmpty())
    {
        fillAuthInfo( m_httpClient, m_credentialsSource == CredentialsSource::serverKey );
    }
    else
    {
        m_credentialsSource = CredentialsSource::remoteUrl;
        m_httpClient->setUserName(remotePeerUrl.userName());
        m_httpClient->setUserPassword(remotePeerUrl.password());
    }

    if (m_localPeer.isServer())
        m_httpClient->addAdditionalHeader(
            Qn::EC2_SYSTEM_ID_HEADER_NAME,
            m_localSystemId.toByteArray());
    if (m_base64EncodeOutgoingTransactions)    //requesting server to encode transactions
        m_httpClient->addAdditionalHeader(
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
            "true");
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME,
        nx_http::header::KeepAlive(
            std::chrono::duration_cast<std::chrono::seconds>(
                m_tcpKeepAliveTimeout)).toString());

    QUrlQuery q;
    {
        QnMutexLocker lk(&m_mutex);
        m_remoteAddr = remotePeerUrl;
        if (!m_remoteAddr.userName().isEmpty())
        {
            m_remoteAddr.setUserName(QString());
            m_remoteAddr.setPassword(QString());
        }
        q = QUrlQuery(m_remoteAddr.query());
    }

#ifdef USE_JSON
    q.addQueryItem( "format", QnLexical::serialized(Qn::JsonFormat) );
#else
    if (m_localPeer.isMobileClient())
        q.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
#endif
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME,
        m_connectionGuid.toByteArray() );
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
        ConnectionType::toString(m_connectionType) );   //incoming means this peer wants to receive data via this connection
    m_httpClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        m_localPeer.instanceId.toByteArray() );
    m_httpClient->addAdditionalHeader(
        Qn::EC2_PROTO_VERSION_HEADER_NAME,
        QByteArray::number(m_localPeerProtocolVersion));

    q.addQueryItem("peerType", QnLexical::serialized(m_localPeer.peerType));

    // add deprecated query item 'isClient' for compatibility with old server version <= 2.6
    if (m_localPeer.peerType == Qn::PT_MobileClient)
        q.addQueryItem("isClient", QString());

    // Client reconnects to the server
    if( m_localPeer.isClient() )
    {
        setState(ConnectingStage2); // one GET method for client peer is enough
        setReadSync(true);
    }

    {
        QnMutexLocker lk(&m_mutex);
        m_remoteAddr.setQuery(q);
    }

    m_httpClient->removeAdditionalHeader( Qn::EC2_CONNECTION_STATE_HEADER_NAME );
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_STATE_HEADER_NAME,
        toString(getState()).toLatin1() );

    nx::utils::Url url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    m_httpClient->doGet(url);
}

void QnTransactionTransportBase::markAsNotSynchronized()
{
    QnMutexLocker lock(&m_mutex);
    m_readSync = false;
    m_writeSync = false;
}

void QnTransactionTransportBase::repeatDoGet()
{
    m_httpClient->removeAdditionalHeader( Qn::EC2_CONNECTION_STATE_HEADER_NAME );
    m_httpClient->addAdditionalHeader( Qn::EC2_CONNECTION_STATE_HEADER_NAME, toString(getState()).toLatin1() );

    nx::utils::Url url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    m_httpClient->doGet(url);
}

void QnTransactionTransportBase::cancelConnecting()
{
    NX_LOG(QnLog::EC2_TRAN_LOG,
        lit("%1 Connection to peer %2 canceled from state %3").
        arg(Q_FUNC_INFO).arg(m_remotePeer.id.toString()).arg(toString(getState())),
        cl_logDEBUG1);
    setState(Error);
}

void QnTransactionTransportBase::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase::onSomeBytesRead. errorCode = %1, bytesRead = %2").
        arg((int)errorCode).arg(bytesRead), cl_logDEBUG2 );

    emit onSomeDataReceivedFromRemotePeer();

    QnMutexLocker lock( &m_mutex );

    m_asyncReadScheduled = false;
    m_lastReceiveTimer.invalidate();

    if( errorCode || bytesRead == 0 )   //error or connection closed
    {
        if( errorCode == SystemError::timedOut )
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Peer %1 timed out. Disconnecting...").arg(m_remotePeer.id.toString()), cl_logWARNING );
        }
        return setStateNoLock( State::Error );
    }

    if (m_state >= QnTransactionTransportBase::Closed)
        return;

    NX_ASSERT( m_state == ReadyForStreaming );

    //parsing and processing input data
    if( !m_incomingTransactionStreamParser->processData( m_readBuffer ) )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error parsing data from peer %1. Disconnecting...").
            arg(m_remotePeer.id.toString()), cl_logWARNING);
        return setStateNoLock( State::Error );
    }

    m_readBuffer.resize(0);

    if( m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME )
        return; //not reading futher while that much transactions are not processed yet

    m_readBuffer.reserve( m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE );
    scheduleAsyncRead();
}

void QnTransactionTransportBase::receivedTransactionNonSafe( const QnByteArrayConstRef& tranDataWithHeader )
{
    if( tranDataWithHeader.isEmpty() )
        return; //it happens in case of keep-alive message

    const QnByteArrayConstRef& tranData = tranDataWithHeader;

    QByteArray serializedTran;
    QnTransactionTransportHeader transportHeader;

    switch( m_remotePeer.dataFormat )
    {
        case Qn::JsonFormat:
            if( !QnJsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(tranData.constData()),
                    static_cast<int>(tranData.size()),
                    transportHeader,
                    serializedTran ) )
            {
                NX_ASSERT( false );
                NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error deserializing JSON data from peer %1. Disconnecting...").
                    arg(m_remotePeer.id.toString()), cl_logWARNING);
                setStateNoLock( State::Error );
                return;
            }
            break;

        case Qn::UbjsonFormat:
            if( !QnUbjsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(tranData.constData()),
                    static_cast<int>(tranData.size()),
                    transportHeader,
                    serializedTran ) )
            {
                NX_ASSERT( false );
                NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error deserializing Ubjson data from peer %1. Disconnecting...").
                    arg(m_remotePeer.id.toString()), cl_logWARNING);
                setStateNoLock( State::Error );
                return;
            }
            break;

        default:
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Received unkown format from peer %1. Disconnecting...").
                arg(m_remotePeer.id.toString()), cl_logWARNING);
            setStateNoLock( State::Error );
            return;
    }

    if (!transportHeader.isNull())
    {
        NX_ASSERT(!transportHeader.processedPeers.empty());
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase::receivedTransactionNonSafe. Got transaction with seq %1 from %2").
            arg(transportHeader.sequence).arg(m_remotePeer.id.toString()), cl_logDEBUG1);
    }

    nx::utils::ObjectDestructionFlag::Watcher watcher(
        &m_connectionFreedFlag);
    emit gotTransaction(
        m_remotePeer.dataFormat,
        std::move(serializedTran),
        transportHeader);
    if (watcher.objectDestroyed())
        return; //connection has been removed by handler

    ++m_postedTranCount;
}

bool QnTransactionTransportBase::hasUnsendData() const
{
    QnMutexLocker lock(&m_mutex);
    return !m_dataToSend.empty();
}

void QnTransactionTransportBase::receivedTransaction(
    const nx_http::HttpHeaders& headers,
    const QnByteArrayConstRef& tranData )
{
    emit onSomeDataReceivedFromRemotePeer();

    QnMutexLocker lock(&m_mutex);

    processChunkExtensions( headers );

    if( nx_http::getHeaderValue(
            headers,
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true" )
    {
        const auto& decodedTranData = QByteArray::fromBase64( tranData.toByteArrayWithRawData() );
        //decodedTranData can contain multiple transactions
        if( !m_sizedDecoder )
        {
            m_sizedDecoder = std::make_shared<nx::utils::bstream::SizedDataDecodingFilter>();
            m_sizedDecoder->setNextFilter( nx::utils::bstream::makeCustomOutputStream(
                std::bind(
                    &QnTransactionTransportBase::receivedTransactionNonSafe,
                    this,
                    std::placeholders::_1 ) ) );
        }
        if( !m_sizedDecoder->processData( decodedTranData ) )
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error parsing data (2) from peer %1. Disconnecting...").
                arg(m_remotePeer.id.toString()), cl_logWARNING);
            return setStateNoLock( State::Error );
        }
    }
    else
    {
        receivedTransactionNonSafe( tranData );
    }
}

void QnTransactionTransportBase::transactionProcessed()
{
    post(
        [this]()
        {
            QnMutexLocker lock(&m_mutex);

            --m_postedTranCount;
            if (m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME)
                m_cond.wakeAll();   //signalling waiters that we are ready for new transactions once again
            if (m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME ||     //not reading futher while that much transactions are not processed yet
                m_asyncReadScheduled ||      //async read is ongoing already, overlapping reads are not supported by sockets api
                m_state > ReadyForStreaming)
            {
                return;
            }

            NX_ASSERT(m_incomingDataSocket || m_outgoingDataSocket);

            m_readBuffer.reserve(m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE);
            scheduleAsyncRead();
        });
}

QnUuid QnTransactionTransportBase::connectionGuid() const
{
    return m_connectionGuid;
}

void QnTransactionTransportBase::setIncomingTransactionChannelSocket(
    QSharedPointer<AbstractCommunicatingSocket> socket,
    const nx_http::Request& /*request*/,
    const QByteArray& requestBuf )
{
    QnMutexLocker lk( &m_mutex );

    NX_ASSERT( m_peerRole == prAccepting );
    NX_ASSERT( m_connectionType != ConnectionType::bidirectional );

    m_incomingDataSocket = std::move(socket);
    m_incomingDataSocket->bindToAioThread(getAioThread());

    //checking transactions format
    if( !m_incomingTransactionStreamParser->processData( requestBuf ) )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error parsing incoming data (3) from peer %1. Disconnecting...").
            arg(m_remotePeer.id.toString()), cl_logWARNING);
        return setStateNoLock( State::Error );
    }

    startListeningNonSafe();
}

void QnTransactionTransportBase::lock()
{
    QnMutexLocker lk(&m_mutex);
    ++m_waiterCount;
}

void QnTransactionTransportBase::unlock()
{
    QnMutexLocker lk(&m_mutex);
    --m_waiterCount;
    m_cond.wakeAll();    //signalling that we are not waiting anymore
}

void QnTransactionTransportBase::waitForNewTransactionsReady()
{
    QnMutexLocker lk( &m_mutex );

    if( m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME && m_state >= ReadyForStreaming)
        return;

    //waiting for some transactions to be processed
    while( (m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME && m_state != Closed) ||
            m_state < ReadyForStreaming)
    {
        m_cond.wait( lk.mutex() );
    }
}

void QnTransactionTransportBase::connectionFailure()
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("Connection to peer %1 failure. Disconnecting...").
        arg(m_remotePeer.id.toString()), cl_logWARNING);
    setState( Error );
}

void QnTransactionTransportBase::setKeepAliveEnabled(bool value)
{
    m_isKeepAliveEnabled = value;
}

void QnTransactionTransportBase::sendHttpKeepAlive()
{
    QnMutexLocker lock(&m_mutex);

    if (m_dataToSend.empty())
    {
        m_dataToSend.push_back( QByteArray() );
        serializeAndSendNextDataBuffer();
    }

    startSendKeepAliveTimerNonSafe();
}

void QnTransactionTransportBase::startSendKeepAliveTimerNonSafe()
{
    if (!m_isKeepAliveEnabled)
        return;

    if (m_peerRole == prAccepting)
    {
        NX_ASSERT(m_outgoingDataSocket);
        m_outgoingDataSocket->registerTimer(
            m_tcpKeepAliveTimeout.count(),
            std::bind(&QnTransactionTransportBase::sendHttpKeepAlive, this));
    }
    else
    {
        //we using http client to send transactions
        m_timer->cancelSync();
        m_timer->start(
            m_tcpKeepAliveTimeout,
            std::bind(&QnTransactionTransportBase::sendHttpKeepAlive, this));
    }
}

void QnTransactionTransportBase::onMonitorConnectionForClosure(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    QnMutexLocker lock( &m_mutex );

    if (errorCode != SystemError::noError && errorCode != SystemError::timedOut)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lit("transaction connection %1 received from %2 failed: %3")
                .arg(m_connectionGuid.toString())
                .arg(m_outgoingDataSocket->getForeignAddress().toString())
                .arg(SystemError::toString(errorCode)),
            cl_logWARNING);
        return setStateNoLock(State::Error);
    }

    if (bytesRead == 0)
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("transaction connection %1 received from %2 has been closed by remote peer").
            arg(m_connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()),
            cl_logWARNING );
        return setStateNoLock( State::Error );
    }

    //TODO #ak should read HTTP responses here and check result code

    using namespace std::placeholders;
    m_dummyReadBuffer.resize( 0 );
    m_outgoingDataSocket->readSomeAsync(
        &m_dummyReadBuffer,
        std::bind(&QnTransactionTransportBase::onMonitorConnectionForClosure, this, _1, _2) );
}

nx::utils::Url QnTransactionTransportBase::generatePostTranUrl()
{
    //return m_postTranBaseUrl;
    nx::utils::Url postTranUrl = m_postTranBaseUrl;
    postTranUrl.setPath( lit("%1/%2").arg(postTranUrl.path()).arg(++m_sentTranSequence) );
    return postTranUrl;
}

void QnTransactionTransportBase::aggregateOutgoingTransactionsNonSafe()
{
    static const int MAX_AGGREGATED_TRAN_SIZE_BYTES = 128*1024;
    //std::deque<DataToSend> m_dataToSend;
    //searching first transaction not being sent currently
    auto saveToIter = std::find_if(
        m_dataToSend.begin(),
        m_dataToSend.end(),
        []( const DataToSend& data )->bool { return data.encodedSourceData.isEmpty(); } );
    if( std::distance( saveToIter, m_dataToSend.end() ) < 2 )
        return; //nothing to aggregate

    //aggregating. Transaction data already contains size
    auto it = std::next(saveToIter);
    for( ;
        it != m_dataToSend.end();
        ++it )
    {
        if( saveToIter->sourceData.size() + it->sourceData.size() > MAX_AGGREGATED_TRAN_SIZE_BYTES )
            break;

        saveToIter->sourceData += it->sourceData;
        it->sourceData.clear();
    }
    //erasing aggregated transactions
    m_dataToSend.erase( std::next(saveToIter), it );
}

bool QnTransactionTransportBase::remotePeerSupportsKeepAlive() const
{
    return m_remotePeerSupportsKeepAlive;
}

bool QnTransactionTransportBase::isHttpKeepAliveTimeout() const
{
    QnMutexLocker lock( &m_mutex );
    return (m_lastReceiveTimer.isValid() &&  //if not valid we still have not begun receiving transactions
         (m_lastReceiveTimer.elapsed() > m_idleConnectionTimeout.count()));
}

void QnTransactionTransportBase::serializeAndSendNextDataBuffer()
{
    NX_ASSERT( !m_dataToSend.empty() );

#ifdef AGGREGATE_TRANSACTIONS_BEFORE_SEND
    if( m_base64EncodeOutgoingTransactions )
        aggregateOutgoingTransactionsNonSafe();
#endif  //AGGREGATE_TRANSACTIONS_BEFORE_SEND

    DataToSend& dataCtx = m_dataToSend.front();

    if( m_base64EncodeOutgoingTransactions )
        dataCtx.sourceData = dataCtx.sourceData.toBase64(); //TODO #ak should use streaming base64 encoder in addDataToTheSendQueue method

    if( dataCtx.encodedSourceData.isEmpty() )
    {
        if( m_peerRole == prAccepting )
        {
            //sending transactions as a response to GET request
            nx_http::HttpHeaders headers;
            headers.emplace(
                "Content-Type",
                m_base64EncodeOutgoingTransactions
                    ? "application/text"
                    : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ) );
            headers.emplace( "Content-Length", nx_http::BufferType::number((int)(dataCtx.sourceData.size())) );
            addHttpChunkExtensions( &headers );

            dataCtx.encodedSourceData.clear();
            dataCtx.encodedSourceData += QByteArray("\r\n--")+TUNNEL_MULTIPART_BOUNDARY+"\r\n"; //TODO #ak move to some variable
            nx_http::serializeHeaders( headers, &dataCtx.encodedSourceData );
            dataCtx.encodedSourceData += "\r\n";
            dataCtx.encodedSourceData += dataCtx.sourceData;

            if( m_compressResponseMsgBody )
            {
                //encoding outgoing message body
                dataCtx.encodedSourceData = nx::utils::bstream::gzip::Compressor::compressData( dataCtx.encodedSourceData );
            }
        }
        else    //m_peerRole == prOriginating
        {
            NX_ASSERT(!m_outgoingDataSocket);
            dataCtx.encodedSourceData = dataCtx.sourceData;
        }
    }
    using namespace std::placeholders;
    NX_LOG( QnLog::EC2_TRAN_LOG,
        lit("Sending data buffer (%1 bytes) to the peer %2").
        arg(dataCtx.encodedSourceData.size()).arg(m_remotePeer.id.toString()), cl_logDEBUG2 );

    if( m_outgoingDataSocket )
    {
        m_outgoingDataSocket->sendAsync(
            dataCtx.encodedSourceData,
            std::bind( &QnTransactionTransportBase::onDataSent, this, _1, _2 ) );
    }
    else  //m_peerRole == prOriginating
    {
        NX_ASSERT( m_peerRole == prOriginating && m_connectionType != ConnectionType::bidirectional );

        //using http client just to authenticate on server
        if( !m_outgoingTranClient )
        {
            using namespace std::chrono;
            m_outgoingTranClient = nx_http::AsyncHttpClient::create();
            m_outgoingTranClient->bindToAioThread(getAioThread());
            m_outgoingTranClient->setSendTimeoutMs(
                duration_cast<milliseconds>(kSocketSendTimeout).count());   //it can take a long time to send large transactions
            m_outgoingTranClient->setResponseReadTimeoutMs(m_idleConnectionTimeout.count());
            m_outgoingTranClient->addAdditionalHeader(
                Qn::EC2_CONNECTION_GUID_HEADER_NAME,
                m_connectionGuid.toByteArray() );
            m_outgoingTranClient->addAdditionalHeader(
                Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
                ConnectionType::toString(ConnectionType::outgoing) );
            if( m_base64EncodeOutgoingTransactions )    //informing server that transaction is encoded
                m_outgoingTranClient->addAdditionalHeader(
                    Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
                    "true" );
            connect(
                m_outgoingTranClient.get(), &nx_http::AsyncHttpClient::done,
                this, &QnTransactionTransportBase::postTransactionDone,
                Qt::DirectConnection );

            if (m_remotePeerCredentials.isNull())
            {
                fillAuthInfo( m_outgoingTranClient, true );
            }
            else
            {
                m_outgoingTranClient->setUserName(m_remotePeerCredentials.user());
                m_outgoingTranClient->setUserPassword(m_remotePeerCredentials.password());
            }

            m_postTranBaseUrl = m_remoteAddr;
            if (m_remotePeer.peerType == Qn::PT_CloudServer)
                m_postTranBaseUrl.setPath(nx::cdb::api::kPushEc2TransactionPath);
            else
                m_postTranBaseUrl.setPath(lit("/ec2/forward_events"));
            m_postTranBaseUrl.setQuery( QString() );
        }

        nx_http::HttpHeaders additionalHeaders;
        addHttpChunkExtensions( &additionalHeaders );
        for( const auto& header: additionalHeaders )
        {
            //removing prev header value (if any)
            m_outgoingTranClient->removeAdditionalHeader( header.first );
            m_outgoingTranClient->addAdditionalHeader( header.first, header.second );
        }

        m_outgoingTranClient->doPost(
            generatePostTranUrl(),
            m_base64EncodeOutgoingTransactions
                ? "application/text"
                : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ),
            dataCtx.encodedSourceData );
    }
}

void QnTransactionTransportBase::onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent )
{
    QnMutexLocker lk( &m_mutex );

    if( errorCode )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Failed to send %1 bytes to %2. %3").arg(m_dataToSend.front().encodedSourceData.size()).
            arg(m_remotePeer.id.toString()).arg(SystemError::toString(errorCode)), cl_logWARNING );
        m_dataToSend.pop_front();
        return setStateNoLock( State::Error );
    }
    NX_ASSERT( bytesSent == (size_t)m_dataToSend.front().encodedSourceData.size() );

    m_dataToSend.pop_front();
    if( m_dataToSend.empty() )
        return;

    serializeAndSendNextDataBuffer();
}

void QnTransactionTransportBase::at_responseReceived(const nx_http::AsyncHttpClientPtr& client)
{
    const int statusCode = client->response()->statusLine.statusCode;

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase::at_responseReceived. statusCode = %1").
        arg(statusCode), cl_logDEBUG2 );

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        m_credentialsSource = (CredentialsSource)((int)m_credentialsSource + 1);
        if (m_credentialsSource < CredentialsSource::none)
        {
            fillAuthInfo( m_httpClient, m_credentialsSource == CredentialsSource::serverKey );
            repeatDoGet();
        }
        else
        {
            QnUuid guid(nx_http::getHeaderValue( client->response()->headers, Qn::EC2_SERVER_GUID_HEADER_NAME ));
            if (!guid.isNull())
            {
                {
                    nx::utils::ObjectDestructionFlag::Watcher watcher(
                        &m_connectionFreedFlag);
                    emit peerIdDiscovered(remoteAddr(), guid);
                    if (watcher.objectDestroyed())
                        return; //connection has been removed by handler
                }

                {
                    nx::utils::ObjectDestructionFlag::Watcher watcher(
                        &m_connectionFreedFlag);
                    emit remotePeerUnauthorized(guid);
                    if (watcher.objectDestroyed())
                        return; //connection has been removed by handler
                }
            }
            cancelConnecting();
        }
        return;
    }

    //saving credentials we used to authorize request
    if (client->request().headers.find(nx_http::header::Authorization::NAME) !=
        client->request().headers.end())
    {
        m_httpAuthCacheItem = client->authCacheItem();
    }

    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find(Qn::EC2_GUID_HEADER_NAME);
    nx_http::HttpHeaders::const_iterator itrRuntimeGuid = client->response()->headers.find(Qn::EC2_RUNTIME_GUID_HEADER_NAME);
    nx_http::HttpHeaders::const_iterator itrSystemIdentityTime = client->response()->headers.find(Qn::EC2_SYSTEM_IDENTITY_HEADER_NAME);
    if (itrSystemIdentityTime != client->response()->headers.end())
        setRemoteIdentityTime(itrSystemIdentityTime->second.toLongLong());

    if (itrGuid == client->response()->headers.end())
    {
        cancelConnecting();
        return;
    }

    nx_http::HttpHeaders::const_iterator ec2CloudHostItr =
        client->response()->headers.find(Qn::EC2_CLOUD_HOST_HEADER_NAME);

    if (!m_localPeer.isMobileClient())
    {
        //checking remote server protocol version
        nx_http::HttpHeaders::const_iterator ec2ProtoVersionIter =
            client->response()->headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);

        m_remotePeerEcProtoVersion =
            ec2ProtoVersionIter == client->response()->headers.end()
            ? nx_ec::INITIAL_EC2_PROTO_VERSION
            : ec2ProtoVersionIter->second.toInt();

        if (m_localPeerProtocolVersion != m_remotePeerEcProtoVersion)
        {
            NX_LOG(lm("Cannot connect to server %1 because of different EC2 proto version. "
                "Local peer version: %2, remote peer version: %3")
                .arg(client->url()).arg(m_localPeerProtocolVersion)
                .arg(m_remotePeerEcProtoVersion),
                cl_logWARNING);
            cancelConnecting();
            return;
        }

        const QString remotePeerCloudHost = ec2CloudHostItr == client->response()->headers.end()
            ? nx::network::AppInfo::defaultCloudHost()
            : QString::fromUtf8(ec2CloudHostItr->second);

        if (nx::network::AppInfo::defaultCloudHost() != remotePeerCloudHost)
        {
            NX_LOG(lm("Cannot connect to server %1 because they have different built in cloud host setting. "
                "Local peer host: %2, remote peer host: %3").
                arg(client->url()).arg(nx::network::AppInfo::defaultCloudHost()).arg(remotePeerCloudHost),
                cl_logWARNING);
            cancelConnecting();
            return;
        }
    }

    m_remotePeer.id = QnUuid(itrGuid->second);
    if (itrRuntimeGuid != client->response()->headers.end())
        m_remotePeer.instanceId = QnUuid(itrRuntimeGuid->second);

    NX_ASSERT(!m_remotePeer.instanceId.isNull());
    if (m_remotePeer.id == kCloudPeerId)
        m_remotePeer.peerType = Qn::PT_CloudServer;
    else if (ec2CloudHostItr == client->response()->headers.end())
        m_remotePeer.peerType = Qn::PT_OldServer; // outgoing connections for server or cloud peers only
    else
        m_remotePeer.peerType = Qn::PT_Server; // outgoing connections for server or cloud peers only
    #ifdef USE_JSON
        m_remotePeer.dataFormat = Qn::JsonFormat;
    #else
        if (m_localPeer.isMobileClient())
            m_remotePeer.dataFormat = Qn::JsonFormat;
        else
            m_remotePeer.dataFormat = Qn::UbjsonFormat;
    #endif

    nx::utils::ObjectDestructionFlag::Watcher watcher(
        &m_connectionFreedFlag);
    emit peerIdDiscovered(remoteAddr(), m_remotePeer.id);
    if (watcher.objectDestroyed())
        return; //connection has been removed by handler

    if (!m_connectionLockGuard)
    {
        NX_CRITICAL(m_connectionGuardSharedState);
        m_connectionLockGuard = std::make_unique<ConnectionLockGuard>(
            m_localPeer.id,
            m_connectionGuardSharedState,
            m_remotePeer.id,
            ConnectionLockGuard::Direction::Outgoing);
    }

    if( !nx_http::StatusCode::isSuccessCode(statusCode) ) //checking that statusCode is 2xx
    {
        cancelConnecting();
        return;
    }

    if (auto password = client->authCacheItem().password)
    {
        m_remotePeerCredentials.setUser(client->authCacheItem().userName);
        m_remotePeerCredentials.setPassword(*password);
    }

    if (getState() == QnTransactionTransportBase::Error || getState() == QnTransactionTransportBase::Closed) {
        return;
    }

    nx_http::HttpHeaders::const_iterator contentTypeIter = client->response()->headers.find("Content-Type");
    if( contentTypeIter == client->response()->headers.end() )
    {
        NX_LOG( lm("Remote transaction server (%1) did not specify Content-Type in response. Aborting connection...")
            .arg(client->url()), cl_logWARNING );
        cancelConnecting();
        return;
    }

    if( !m_multipartContentParser->setContentType( contentTypeIter->second ) )
    {
        NX_LOG( lm("Remote transaction server (%1) specified Content-Type (%2) which does not define multipart HTTP content")
            .arg(client->url()).arg(QLatin1String(contentTypeIter->second)), cl_logWARNING );
        cancelConnecting();
        return;
    }

    //TODO #ak check Content-Type (to support different transports)

    auto contentEncodingIter = client->response()->headers.find("Content-Encoding");
    if( contentEncodingIter != client->response()->headers.end() )
    {
        if( contentEncodingIter->second == "gzip" )
        {
            //enabling decompression of received transactions
            auto ungzip = std::make_shared<nx::utils::bstream::gzip::Uncompressor>();
            ungzip->setNextFilter( std::move(m_incomingTransactionStreamParser) );
            m_incomingTransactionStreamParser = std::move(ungzip);
        }
        else
        {
            //TODO #ak unsupported Content-Encoding ?
        }
    }

    auto isCloudIter = client->response()->headers.find("X-Nx-Cloud");
    if (isCloudIter != client->response()->headers.end())
        setState(ConnectingStage2);

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();

    if (getState() == ConnectingStage1)
    {
        if (m_connectionLockGuard->tryAcquireConnecting())
        {
            setState(ConnectingStage2);
            NX_ASSERT( data.isEmpty() );
        }
        else
        {
            QnMutexLocker lk( &m_mutex );
            QUrlQuery query = QUrlQuery(m_remoteAddr.toQUrl());
            query.addQueryItem("canceled", QString());
            m_remoteAddr.setQuery(query);
        }
        repeatDoGet();
    }
    else {
        if( nx_http::getHeaderValue(
                m_httpClient->response()->headers,
                Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true" )
        {
            //inserting base64 decoder before the last filter
            m_incomingTransactionStreamParser = nx::utils::bstream::insert(
                m_incomingTransactionStreamParser,
                nx::utils::bstream::last( m_incomingTransactionStreamParser ),
                std::make_shared<Base64DecoderFilter>() );

            //base64-encoded data contains multiple transactions so
            //    inserting sized data decoder after base64 decoder
            m_incomingTransactionStreamParser = nx::utils::bstream::insert(
                m_incomingTransactionStreamParser,
                nx::utils::bstream::last( m_incomingTransactionStreamParser ),
                std::make_shared<nx::utils::bstream::SizedDataDecodingFilter>() );
        }

        auto keepAliveHeaderIter = m_httpClient->response()->headers.find(Qn::EC2_CONNECTION_TIMEOUT_HEADER_NAME);
        if (keepAliveHeaderIter != m_httpClient->response()->headers.end())
        {
            m_remotePeerSupportsKeepAlive = true;
            nx_http::header::KeepAlive keepAliveHeader;
            if (keepAliveHeader.parse(keepAliveHeaderIter->second))
                m_tcpKeepAliveTimeout = std::max(
                    std::chrono::duration_cast<std::chrono::seconds>(m_tcpKeepAliveTimeout),
                    keepAliveHeader.timeout);
        }

        m_incomingDataSocket = QSharedPointer<AbstractCommunicatingSocket>(m_httpClient->takeSocket().release());
        if( m_connectionType == ConnectionType::bidirectional )
            m_outgoingDataSocket = m_incomingDataSocket;

        {
            QnMutexLocker lk( &m_mutex );
            startSendKeepAliveTimerNonSafe();
        }

        m_httpClient.reset();
        if (m_connectionLockGuard->tryAcquireConnected())
        {
            setExtraDataBuffer(data);
            setState(QnTransactionTransportBase::Connected);
        }
        else
        {
            cancelConnecting();
        }
    }
}

void QnTransactionTransportBase::at_httpClientDone( const nx_http::AsyncHttpClientPtr& client )
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransportBase::at_httpClientDone. state = %1").
        arg((int)client->state()), cl_logDEBUG2 );

    nx_http::AsyncClient::State state = client->state();
    if( state == nx_http::AsyncClient::State::sFailed ) {
        cancelConnecting();
    }
}

void QnTransactionTransportBase::processTransactionData(const QByteArray& data)
{
    NX_ASSERT( m_peerRole == prOriginating );
    if( !m_incomingTransactionStreamParser->processData( data ) )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error processing incoming data (4) from peer %1. Disconnecting...").
            arg(m_remotePeer.id.toString()), cl_logWARNING);
        return setStateNoLock( State::Error );
    }
}

bool QnTransactionTransportBase::isReadyToSend(ApiCommand::Value command) const
{
    if (m_state == ReadyForStreaming) {
        // allow to send system command immediately, without tranSyncRequest
        return (command != ApiCommand::NotDefined && ApiCommand::isSystem(command)) ? true : m_writeSync;
    }
    else {
        return false;
    }
}

bool QnTransactionTransportBase::isReadSync(ApiCommand::Value command) const
{
    if (m_state == ReadyForStreaming) {
        // allow to read system command immediately, without tranSyncRequest
        return (command != ApiCommand::NotDefined && ApiCommand::isSystem(command)) ? true : m_readSync;
    }
    else {
        return false;
    }
}

QString QnTransactionTransportBase::toString( State state )
{
    switch( state )
    {
        case NotDefined:
            return lit("NotDefined");
        case ConnectingStage1:
            return lit("ConnectingStage1");
        case ConnectingStage2:
            return lit("ConnectingStage2");
        case Connected:
            return lit("Connected");
        case NeedStartStreaming:
            return lit("NeedStartStreaming");
        case ReadyForStreaming:
            return lit("ReadyForStreaming");
        case Closed:
            return lit("Closed");
        case Error:
            return lit("Error");
        default:
            return lit("unknown");
    }
}

void QnTransactionTransportBase::addHttpChunkExtensions( nx_http::HttpHeaders* const headers )
{
    for( auto val: m_beforeSendingChunkHandlers )
        val.second( this, headers );
}

void QnTransactionTransportBase::processChunkExtensions( const nx_http::HttpHeaders& headers )
{
    if( headers.empty() )
        return;

    for( auto val: m_httpChunkExtensonHandlers )
        val.second( this, headers );
}

void QnTransactionTransportBase::setExtraDataBuffer(const QByteArray& data)
{
    QnMutexLocker lk( &m_mutex );
    NX_ASSERT(m_extraData.isEmpty());
    m_extraData = data;
}

void QnTransactionTransportBase::setRemoteIdentityTime(qint64 time)
{
    m_remoteIdentityTime = time;
}

qint64 QnTransactionTransportBase::remoteIdentityTime() const
{
    return m_remoteIdentityTime;
}

void QnTransactionTransportBase::scheduleAsyncRead()
{
    if (!m_incomingDataSocket)
        return;

    NX_EXPECT(isInSelfAioThread());
    NX_ASSERT(!m_asyncReadScheduled);

    using namespace std::placeholders;
    m_incomingDataSocket->readSomeAsync(
        &m_readBuffer,
        std::bind(&QnTransactionTransportBase::onSomeBytesRead, this, _1, _2));
    m_asyncReadScheduled = true;
    m_lastReceiveTimer.restart();
}

void QnTransactionTransportBase::startListeningNonSafe()
{
    NX_ASSERT( m_incomingDataSocket || m_outgoingDataSocket );
    m_httpStreamReader.resetState();

    post(
        [this]()
        {
            using namespace std::chrono;
            using namespace std::placeholders;

            if (!m_incomingDataSocket)
                return;

            m_incomingDataSocket->setRecvTimeout(SOCKET_TIMEOUT);
            m_incomingDataSocket->setSendTimeout(
                duration_cast<milliseconds>(kSocketSendTimeout).count());
            m_incomingDataSocket->setNonBlockingMode(true);
            m_readBuffer.reserve( m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE );

            scheduleAsyncRead();
        });
}

void QnTransactionTransportBase::postTransactionDone( const nx_http::AsyncHttpClientPtr& client )
{
    QnMutexLocker lk( &m_mutex );

    NX_ASSERT( client == m_outgoingTranClient );

    if( client->failed() || !client->response() )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lit("Network error posting transaction to %1. system result code: %2")
                .arg(m_postTranBaseUrl.toString())
                .arg(SystemError::toString(client->lastSysErrorCode())),
            cl_logWARNING);
        setStateNoLock( Error );
        return;
    }

    DataToSend& dataCtx = m_dataToSend.front();

    if( client->response()->statusLine.statusCode == nx_http::StatusCode::unauthorized &&
        m_authOutgoingConnectionByServerKey )
    {
        NX_LOG(
            QnLog::EC2_TRAN_LOG,
            lit("Failed to authenticate on peer %1 by key. Retrying using admin credentials...").arg(m_postTranBaseUrl.toString()),
            cl_logDEBUG2 );
        m_authOutgoingConnectionByServerKey = false;
        fillAuthInfo( m_outgoingTranClient, m_authOutgoingConnectionByServerKey );
        m_outgoingTranClient->doPost(
            m_postTranBaseUrl,
            m_base64EncodeOutgoingTransactions
                ? "application/text"
                : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ),
            dataCtx.encodedSourceData );
        return;
    }

    if( client->response()->statusLine.statusCode != nx_http::StatusCode::ok )
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("Server %1 returned %2 (%3) response while posting transaction").
            arg(m_postTranBaseUrl.toString()).arg(client->response()->statusLine.statusCode).
            arg(QLatin1String(client->response()->statusLine.reasonPhrase)), cl_logWARNING );
        setStateNoLock( Error );
        m_outgoingTranClient.reset();
        return;
    }

    m_dataToSend.pop_front();
    if( m_dataToSend.empty() )
        return;

    serializeAndSendNextDataBuffer();
}

}
