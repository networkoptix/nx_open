
#include "transaction_transport.h"

#include <atomic>

#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <nx_ec/ec_proto_version.h>
#include <utils/bsf/sized_data_decoder.h>
#include <utils/common/timermanager.h>
#include <utils/gzip/gzip_compressor.h>
#include <utils/gzip/gzip_uncompressor.h>
#include <utils/media/custom_output_stream.h>
#include <utils/network/http/base64_decoder_filter.h>

#include "transaction_message_bus.h"
#include "utils/common/log.h"
#include "utils/common/util.h"
#include "utils/common/systemerror.h"
#include "utils/network/socket_factory.h"
#include "transaction_log.h"
#include <transaction/chunked_transfer_encoder.h>
#include "common/common_module.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "api/app_server_connection.h"
#include "core/resource/user_resource.h"
#include "api/global_settings.h"
#include "database/db_manager.h"
#include "http/custom_headers.h"
#include "version.h"

//#define USE_SINGLE_TWO_WAY_CONNECTION
//!if not defined, ubjson is used
//#define USE_JSON
#define ENCODE_TO_BASE64
//#define PIPELINE_POST_REQUESTS


/*!
    Real number of transactions posted to the QnTransactionMessageBus can be greater 
    (all transactions that read with single read from socket)
*/
static const int MAX_TRANS_TO_POST_AT_A_TIME = 16;

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
const char* QnTransactionTransport::TUNNEL_MULTIPART_BOUNDARY = "ec2boundary";
const char* QnTransactionTransport::TUNNEL_CONTENT_TYPE = "multipart/mixed; boundary=ec2boundary";

QSet<QnUuid> QnTransactionTransport::m_existConn;
QnTransactionTransport::ConnectingInfoMap QnTransactionTransport::m_connectingConn;
QnMutex QnTransactionTransport::m_staticMutex;

void QnTransactionTransport::default_initializer()
{
    //TODO #ak make a default constructor of it after move to msvc2013
    m_lastConnectTime = 0;
    m_readSync = false;
    m_writeSync = false;
    m_syncDone = false;
    m_syncInProgress = false;
    m_needResync = false;
    m_state = NotDefined; 
    m_connected = false;
    m_prevGivenHandlerID = 0;
    m_authByKey = true;
    m_postedTranCount = 0;
    m_asyncReadScheduled = false;
    m_remoteIdentityTime = 0;
    m_connectionType = ConnectionType::none;
    m_peerRole = prOriginating;
    m_compressResponseMsgBody = false;
    m_authOutgoingConnectionByServerKey = true;
    m_sendKeepAliveTask = 0;
    m_base64EncodeOutgoingTransactions = false;
    m_sentTranSequence = 0;
    m_waiterCount = 0;
}

QnTransactionTransport::QnTransactionTransport(
    const QnUuid& connectionGuid,
    const ApiPeerData& localPeer,
    const ApiPeerData& remotePeer,
    QSharedPointer<AbstractStreamSocket> socket,
    ConnectionType::Type connectionType,
    const nx_http::Request& request,
    const QByteArray& contentEncoding )
{
    default_initializer();

    m_localPeer = localPeer;
    m_remotePeer = remotePeer;
    m_outgoingDataSocket = std::move(socket);
    m_connectionType = connectionType;
    m_peerRole = prAccepting;
    m_contentEncoding = contentEncoding;
    m_connectionGuid = connectionGuid;

    if( !m_outgoingDataSocket->setSendTimeout( TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE ) )
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lit("Error setting socket write timeout for transaction connection %1 received from %2").
            arg(connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()).
            arg(SystemError::toString(osErrorCode)), cl_logWARNING );
    }

    //TODO #ak use binary filter stream for serializing transactions
    m_base64EncodeOutgoingTransactions = nx_http::getHeaderValue(
        request.headers, Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true";

    if( m_connectionType == ConnectionType::bidirectional )
        m_incomingDataSocket = m_outgoingDataSocket;

    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    using namespace std::placeholders;
    if( m_contentEncoding == "gzip" )
    {
        m_compressResponseMsgBody = true;
    }
    
    //creating parser sequence: http_msg_stream_parser -> ext_headers_processor -> transaction handler
    auto incomingTransactionsRequestsParser = std::make_shared<nx_http::HttpMessageStreamParser>();
    std::weak_ptr<nx_http::HttpMessageStreamParser> incomingTransactionsRequestsParserWeak(
        incomingTransactionsRequestsParser );

    auto extensionHeadersProcessor = makeFilterWithFunc( //this filter receives single HTTP message 
        [this, incomingTransactionsRequestsParserWeak]() {
            if( auto incomingTransactionsRequestsParserStrong = incomingTransactionsRequestsParserWeak.lock() )
                processChunkExtensions( incomingTransactionsRequestsParserStrong->currentMessage().headers() );
        } );

    extensionHeadersProcessor->setNextFilter( makeCustomOutputStream(
        std::bind(
            &QnTransactionTransport::receivedTransactionNonSafe,
            this,
            std::placeholders::_1 ) ) );

    incomingTransactionsRequestsParser->setNextFilter( std::move(extensionHeadersProcessor) );

    m_incomingTransactionStreamParser = std::move(incomingTransactionsRequestsParser);

    startSendKeepAliveTimerNonSafe();

    //monitoring m_outgoingDataSocket for connection close
    m_dummyReadBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    if( !m_outgoingDataSocket->readSomeAsync(
            &m_dummyReadBuffer,
            std::bind(&QnTransactionTransport::monitorConnectionForClosure, this, _1, _2) ) )
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QnLog::EC2_TRAN_LOG,
            lit("Error scheduling async read on transaction connection %1 received from %2. %3").
            arg(connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()).
            arg(SystemError::toString(osErrorCode)), cl_logWARNING );
    }
}

QnTransactionTransport::QnTransactionTransport( const ApiPeerData &localPeer )
{
    //TODO #ak msvc2013 delegate constructor
    default_initializer();

    m_localPeer = localPeer;
    m_connectionType = 
#ifdef USE_SINGLE_TWO_WAY_CONNECTION
        ConnectionType::bidirectional
#else
        ConnectionType::incoming
#endif
        ;
    m_peerRole = prOriginating;
    m_connectionGuid = QnUuid::createUuid();
#ifdef ENCODE_TO_BASE64
    m_base64EncodeOutgoingTransactions = true;
#endif

    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    //creating parser sequence: multipart_parser -> ext_headers_processor -> transaction handler
    m_multipartContentParser = std::make_shared<nx_http::MultipartContentParser>();
    std::weak_ptr<nx_http::MultipartContentParser> multipartContentParserWeak( m_multipartContentParser );
    auto extensionHeadersProcessor = makeFilterWithFunc( //this filter receives single multipart message 
        [this, multipartContentParserWeak]() {
            if( auto multipartContentParser = multipartContentParserWeak.lock() )
                processChunkExtensions( multipartContentParser->prevFrameHeaders() );
        } );
    extensionHeadersProcessor->setNextFilter( makeCustomOutputStream(
        std::bind(
            &QnTransactionTransport::receivedTransactionNonSafe,
            this,
            std::placeholders::_1 ) ) );
    m_multipartContentParser->setNextFilter( std::move(extensionHeadersProcessor) );

    m_incomingTransactionStreamParser = m_multipartContentParser;
}

QnTransactionTransport::~QnTransactionTransport()
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("~QnTransactionTransport for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    quint64 sendKeepAliveTaskLocal = 0;
    {
        QnMutexLocker lock(&m_mutex);
        sendKeepAliveTaskLocal = m_sendKeepAliveTask;
        m_sendKeepAliveTask = 0;    //no new task can be added
    }
    if( sendKeepAliveTaskLocal )
        TimerManager::instance()->joinAndDeleteTimer( sendKeepAliveTaskLocal );

    {
        auto httpClientLocal = m_httpClient;
        if( httpClientLocal )
            httpClientLocal->terminate();
    }
    {
        auto outgoingTranClientLocal = m_outgoingTranClient;
        if( outgoingTranClientLocal )
            outgoingTranClientLocal->terminate();
    }

    {
        QnMutexLocker lk( &m_mutex );
        m_state = Closed;
        m_cond.wakeAll();   //signalling waiters that connection is being closed
        //waiting for waiters to quit
        while( m_waiterCount > 0 )
            m_cond.wait( lk.mutex() );
    }
    closeSocket();
    //not calling QnTransactionTransport::close since it will emit stateChanged, 
        //which can potentially lead to some trouble

    if (m_connected)
        connectDone(m_remotePeer.id);

    if (m_ttFinishCallback)
        m_ttFinishCallback();
}

void QnTransactionTransport::addData(QByteArray data)
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
        data.clear();   //cause I can!
        m_dataToSend.push_back( std::move(dataWithSize) );
    }
    else
    {
        m_dataToSend.push_back( std::move( data ) );
    }
    if( m_dataToSend.size() == 1 )
        serializeAndSendNextDataBuffer();
}

void QnTransactionTransport::closeSocket()
{
    if( m_outgoingDataSocket )
    {
        m_outgoingDataSocket->terminateAsyncIO( true );
        m_outgoingDataSocket->close();
    }

    if( m_incomingDataSocket &&
        m_incomingDataSocket != m_outgoingDataSocket )   //they are equal in case of bidirectional connection
    {
        m_incomingDataSocket->terminateAsyncIO( true );
        m_incomingDataSocket->close();
    }

    m_outgoingDataSocket.reset();
    m_incomingDataSocket.reset();
}

void QnTransactionTransport::setState(State state)
{
    QnMutexLocker lock( &m_mutex );
    setStateNoLock(state);
}

void QnTransactionTransport::processExtraData()
{
    QnMutexLocker lock( &m_mutex );
    if( !m_extraData.isEmpty() )
    {
        processTransactionData(m_extraData);
        m_extraData.clear();
    }
}

void QnTransactionTransport::startListening()
{
    QnMutexLocker lock( &m_mutex );
    startListeningNonSafe();
}

void QnTransactionTransport::setStateNoLock(State state)
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
        emit stateChanged(state);
    }
    m_cond.wakeAll();
}

QUrl QnTransactionTransport::remoteAddr() const
{
    QnMutexLocker lock(&m_mutex);
    // Emulating deep copy here
    QUrl tmpUrl(m_remoteAddr);
    tmpUrl.setUserName(tmpUrl.userName());
    return tmpUrl;
}

SocketAddress QnTransactionTransport::remoteSocketAddr() const
{
    QnMutexLocker lock(&m_mutex);
    SocketAddress addr = SocketAddress(
        m_remoteAddr.host(),
        m_remoteAddr.port()
    );

    return addr;
}

nx_http::AuthInfoCache::AuthorizationCacheItem QnTransactionTransport::authData() const
{
    QnMutexLocker lock( &m_mutex );
    return m_httpAuthCacheItem;
}

QnTransactionTransport::State QnTransactionTransport::getState() const
{
    QnMutexLocker lock( &m_mutex );
    return m_state;
}

bool QnTransactionTransport::isIncoming() const {
    QnMutexLocker lock(&m_mutex);
    return m_peerRole == prAccepting;
}

int QnTransactionTransport::setHttpChunkExtensonHandler( HttpChunkExtensonHandler eventHandler )
{
    QnMutexLocker lk( &m_mutex );
    m_httpChunkExtensonHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

int QnTransactionTransport::setBeforeSendingChunkHandler( BeforeSendingChunkHandler eventHandler )
{
    QnMutexLocker lk( &m_mutex );
    m_beforeSendingChunkHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

void QnTransactionTransport::removeEventHandler( int eventHandlerID )
{
    QnMutexLocker lk( &m_mutex );
    m_httpChunkExtensonHandlers.erase( eventHandlerID );
    m_beforeSendingChunkHandlers.erase( eventHandlerID );
}

QSharedPointer<AbstractStreamSocket> QnTransactionTransport::getSocket() const
{
    if( m_connectionType == ConnectionType::bidirectional )
    {
        return m_incomingDataSocket;
    }
    else
    {
        if( m_peerRole == prOriginating )
            return m_incomingDataSocket;
        else
            return m_outgoingDataSocket;
    }
}

void QnTransactionTransport::close()
{
    setState(State::Closed);    //changing state before freeing socket so that everyone 
                                //stop using socket before it is actually freed
 
    closeSocket();
    {
        QnMutexLocker lock( &m_mutex );
        assert( !m_incomingDataSocket && !m_outgoingDataSocket );
        m_readSync = false;
        m_writeSync = false;
    }
}

void QnTransactionTransport::fillAuthInfo( const nx_http::AsyncHttpClientPtr& httpClient, bool authByKey )
{
    if (!QnAppServerConnectionFactory::videowallGuid().isNull()) {
        httpClient->addAdditionalHeader("X-NetworkOptix-VideoWall", QnAppServerConnectionFactory::videowallGuid().toString().toUtf8());
        return;
    }

    QnMediaServerResourcePtr ownServer = qnResPool->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    if (ownServer && authByKey) 
    {
        httpClient->setUserName(ownServer->getId().toString().toLower());    
        httpClient->setUserPassword(ownServer->getAuthKey());
    }
    else {
        QUrl url = QnAppServerConnectionFactory::url();
        httpClient->setUserName(url.userName().toLower());
        if (dbManager) {
            QnUserResourcePtr adminUser = QnGlobalSettings::instance()->getAdminUser();
            if (adminUser) {
                httpClient->setUserPassword(adminUser->getDigest());
                httpClient->setAuthType(nx_http::AsyncHttpClient::authDigestWithPasswordHash);
            }
        }
        else {
            httpClient->setUserPassword(url.password());
        }
    }
}

void QnTransactionTransport::doOutgoingConnect(const QUrl& remotePeerUrl)
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::doOutgoingConnect. remotePeerUrl = %1").
        arg(remotePeerUrl.toString()), cl_logDEBUG2 );

    setState(ConnectingStage1);

    m_httpClient = nx_http::AsyncHttpClient::create();
    m_httpClient->setSendTimeoutMs( TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE );
    m_httpClient->setResponseReadTimeoutMs( TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE );
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &QnTransactionTransport::at_responseReceived,
        Qt::DirectConnection);
    connect(
        m_httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &QnTransactionTransport::at_httpClientDone,
        Qt::DirectConnection);
    
    fillAuthInfo( m_httpClient, m_authByKey );
    if( m_localPeer.isServer() )
        m_httpClient->addAdditionalHeader(
            Qn::EC2_SYSTEM_NAME_HEADER_NAME,
            QnCommonModule::instance()->localSystemName().toUtf8() );
    if( m_base64EncodeOutgoingTransactions )    //requesting server to encode transactions
        m_httpClient->addAdditionalHeader(
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
            "true" );

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
#endif
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME,
        m_connectionGuid.toByteArray() );
    m_httpClient->addAdditionalHeader(
        Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
        ConnectionType::toString(m_connectionType) );   //incoming means this peer wants to receive data via this connection
    m_httpClient->addAdditionalHeader(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        qnCommon->runningInstanceGUID().toByteArray() );
    

    // Client reconnects to the server
    if( m_localPeer.isClient() ) {
        q.removeQueryItem("isClient");
        q.addQueryItem("isClient", QString());
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

    QUrl url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    if (!m_httpClient->doGet(url)) {
        qWarning() << Q_FUNC_INFO << "Failed to execute m_httpClient->doGet. Reconnect transaction transport";
        setState(Error);
    }
}

bool QnTransactionTransport::tryAcquireConnecting(const QnUuid& remoteGuid, bool isOriginator)
{
    QnMutexLocker lock( &m_staticMutex );

    Q_ASSERT(!remoteGuid.isNull());

    bool isExist = m_existConn.contains(remoteGuid);
    isExist |= isOriginator ?  m_connectingConn.value(remoteGuid).first : m_connectingConn.value(remoteGuid).second;
    bool isTowardConnecting = isOriginator ?  m_connectingConn.value(remoteGuid).second : m_connectingConn.value(remoteGuid).first;
    bool fail = isExist || (isTowardConnecting && remoteGuid.toRfc4122() > qnCommon->moduleGUID().toRfc4122());
    if (!fail) {
        if (isOriginator)
            m_connectingConn[remoteGuid].first = true;
        else
            m_connectingConn[remoteGuid].second = true;
    }
    return !fail;
}

void QnTransactionTransport::connectingCanceled(const QnUuid& remoteGuid, bool isOriginator)
{
    QnMutexLocker lock( &m_staticMutex );
    connectingCanceledNoLock(remoteGuid, isOriginator);
}

void QnTransactionTransport::connectingCanceledNoLock(const QnUuid& remoteGuid, bool isOriginator)
{
    ConnectingInfoMap::iterator itr = m_connectingConn.find(remoteGuid);
    if (itr != m_connectingConn.end()) {
        if (isOriginator)
            itr.value().first = false;
        else
            itr.value().second = false;
        if (!itr.value().first && !itr.value().second)
            m_connectingConn.erase(itr);
    }
}

bool QnTransactionTransport::tryAcquireConnected(const QnUuid& remoteGuid, bool isOriginator)
{
    QnMutexLocker lock( &m_staticMutex );
    bool isExist = m_existConn.contains(remoteGuid);
    bool isTowardConnecting = isOriginator ?  m_connectingConn.value(remoteGuid).second : m_connectingConn.value(remoteGuid).first;
    bool fail = isExist || (isTowardConnecting && remoteGuid.toRfc4122() > qnCommon->moduleGUID().toRfc4122());
    if (!fail) {
        m_existConn << remoteGuid;
        connectingCanceledNoLock(remoteGuid, isOriginator);
    }
    return !fail;    
}

void QnTransactionTransport::connectDone(const QnUuid& id)
{
    QnMutexLocker lock( &m_staticMutex );
    m_existConn.remove(id);
}

void QnTransactionTransport::repeatDoGet()
{
    m_httpClient->removeAdditionalHeader( Qn::EC2_CONNECTION_STATE_HEADER_NAME );
    m_httpClient->addAdditionalHeader( Qn::EC2_CONNECTION_STATE_HEADER_NAME, toString(getState()).toLatin1() );
    
    QUrl url = remoteAddr();
    url.setPath(url.path() + lit("/") + toString(getState()));
    if (!m_httpClient->doGet(url))
        cancelConnecting();
}

void QnTransactionTransport::cancelConnecting()
{
    if (getState() == ConnectingStage2)
        QnTransactionTransport::connectingCanceled(m_remotePeer.id, true);
    NX_LOG(QnLog::EC2_TRAN_LOG, 
        lit("%1 Connection to peer %2 canceled from state %3").
        arg(Q_FUNC_INFO).arg(m_remotePeer.id.toString()).arg(toString(getState())),
        cl_logDEBUG1);
    setState(Error);
}

void QnTransactionTransport::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::onSomeBytesRead. errorCode = %1, bytesRead = %2").
        arg((int)errorCode).arg(bytesRead), cl_logDEBUG2 );
    
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

    if (m_state >= QnTransactionTransport::Closed)
        return;

    assert( m_state == ReadyForStreaming );

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

void QnTransactionTransport::receivedTransactionNonSafe( const QnByteArrayConstRef& tranDataWithHeader )
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
                    tranData.size(),
                    transportHeader,
                    serializedTran ) )
            {
                Q_ASSERT( false );
                NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error deserializing JSON data from peer %1. Disconnecting...").
                    arg(m_remotePeer.id.toString()), cl_logWARNING);
                setStateNoLock( State::Error );
                return;
            }
            break;

        case Qn::UbjsonFormat:
            if( !QnUbjsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(tranData.constData()),
                    tranData.size(),
                    transportHeader,
                    serializedTran ) )
            {
                Q_ASSERT( false );
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

    Q_ASSERT( !transportHeader.processedPeers.empty() );
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::receivedTransactionNonSafe. Got transaction with seq %1 from %2").
        arg(transportHeader.sequence).arg(m_remotePeer.id.toString()), cl_logDEBUG1);
    emit gotTransaction( m_remotePeer.dataFormat, serializedTran, transportHeader);
    ++m_postedTranCount;
}

bool QnTransactionTransport::hasUnsendData() const
{
    QnMutexLocker lock(&m_mutex);
    return !m_dataToSend.empty();
}

void QnTransactionTransport::receivedTransaction(
    const nx_http::HttpHeaders& headers,
    const QnByteArrayConstRef& tranData )
{
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
            m_sizedDecoder = std::make_shared<nx_bsf::SizedDataDecodingFilter>();
            m_sizedDecoder->setNextFilter( makeCustomOutputStream(
                std::bind(
                    &QnTransactionTransport::receivedTransactionNonSafe,
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

void QnTransactionTransport::transactionProcessed()
{
    QnMutexLocker lock( &m_mutex );

    --m_postedTranCount;
    if( m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME )
        m_cond.wakeAll();   //signalling waiters that we are ready for new transactions once again
    if( m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME ||     //not reading futher while that much transactions are not processed yet
        m_asyncReadScheduled ||      //async read is ongoing already, overlapping reads are not supported by sockets api
        m_state > ReadyForStreaming )
    {
        return;
    }

    assert( m_incomingDataSocket || m_outgoingDataSocket );

    m_readBuffer.reserve( m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE );
    scheduleAsyncRead();
}

QnUuid QnTransactionTransport::connectionGuid() const
{
    return m_connectionGuid;
}

void QnTransactionTransport::setIncomingTransactionChannelSocket(
    QSharedPointer<AbstractStreamSocket> socket,
    const nx_http::Request& /*request*/,
    const QByteArray& requestBuf )
{
    QnMutexLocker lk( &m_mutex );

    assert( m_peerRole == prAccepting );
    assert( m_connectionType != ConnectionType::bidirectional );
    
    m_incomingDataSocket = std::move(socket);

    //checking transactions format
    if( !m_incomingTransactionStreamParser->processData( requestBuf ) )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error parsing incoming data (3) from peer %1. Disconnecting...").
            arg(m_remotePeer.id.toString()), cl_logWARNING);
        return setStateNoLock( State::Error );
    }

    startListeningNonSafe();
}

void QnTransactionTransport::waitForNewTransactionsReady( std::function<void()> invokeBeforeWait )
{
    QnMutexLocker lk( &m_mutex );

    if( m_postedTranCount < MAX_TRANS_TO_POST_AT_A_TIME && m_state >= ReadyForStreaming)
        return;

    //waiting for some transactions to be processed
    ++m_waiterCount;
    if( invokeBeforeWait )
        invokeBeforeWait();
    while( (m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME && m_state != Closed) ||
            m_state < ReadyForStreaming)
    {
        m_cond.wait( lk.mutex() );
    }
    --m_waiterCount;
    m_cond.wakeAll();    //signalling that we are not waiting anymore
}

void QnTransactionTransport::connectionFailure()
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("Connection to peer %1 failure. Disconnecting...").
        arg(m_remotePeer.id.toString()), cl_logWARNING);
    setState( Error );
}

void QnTransactionTransport::sendHttpKeepAlive( quint64 taskID )
{
    QnMutexLocker lock(&m_mutex);

    if( m_sendKeepAliveTask != taskID )
        return; //task has been cancelled

    if (m_dataToSend.empty())
    {
        m_dataToSend.push_back( QByteArray() );
        serializeAndSendNextDataBuffer();
    }

    startSendKeepAliveTimerNonSafe();
}

void QnTransactionTransport::startSendKeepAliveTimerNonSafe()
{
    if( !m_remotePeer.isServer() )
        return; //not sending keep-alive to a client

    if( m_peerRole == prAccepting )
    {
        assert( m_outgoingDataSocket );
        if( !m_outgoingDataSocket->registerTimer(
                TCP_KEEPALIVE_TIMEOUT,
                std::bind(&QnTransactionTransport::sendHttpKeepAlive, this, 0) ) )
        {
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error registering internal time. peer %1. Disconnecting...").
                arg(m_remotePeer.id.toString()), cl_logWARNING);
            setStateNoLock( State::Error );
        }
    }
    else
    {
        //we using http client to send transactions
        m_sendKeepAliveTask = TimerManager::instance()->addTimer(
            std::bind(&QnTransactionTransport::sendHttpKeepAlive, this, std::placeholders::_1),
            TCP_KEEPALIVE_TIMEOUT );
    }
}

void QnTransactionTransport::monitorConnectionForClosure(
    SystemError::ErrorCode errorCode,
    size_t bytesRead )
{
    QnMutexLocker lock( &m_mutex );

    if( errorCode != SystemError::noError && errorCode != SystemError::timedOut )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("transaction connection %1 received from %2 has been closed by remote peer").
            arg(m_connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()), cl_logWARNING );
        return setStateNoLock( State::Error );
    }

    if( bytesRead == 0 )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("transaction connection %1 received from %2 has been closed").
            arg(m_connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()),
            cl_logWARNING );
        return setStateNoLock( State::Error );
    }

    //TODO #ak should read HTTP responses here and check result code

    using namespace std::placeholders;
    m_dummyReadBuffer.resize( 0 );
    if( !m_outgoingDataSocket->readSomeAsync(
            &m_dummyReadBuffer,
            std::bind(&QnTransactionTransport::monitorConnectionForClosure, this, _1, _2) ) )
    {
        const auto osErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error scheduling async read on transaction connection %1 received from %2. %3").
            arg(m_connectionGuid.toString()).arg(m_outgoingDataSocket->getForeignAddress().toString()).
            arg(SystemError::toString(osErrorCode)), cl_logWARNING );
    }
}

QUrl QnTransactionTransport::generatePostTranUrl()
{
    QUrl postTranUrl = m_postTranBaseUrl;
    postTranUrl.setPath( lit("%1/%2").arg(postTranUrl.path()).arg(++m_sentTranSequence) );
    return postTranUrl;
}

void QnTransactionTransport::aggregateOutgoingTransactionsNonSafe()
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

bool QnTransactionTransport::isHttpKeepAliveTimeout() const
{
    QnMutexLocker lock( &m_mutex );
    return (m_lastReceiveTimer.isValid() &&  //if not valid we still have not begun receiving transactions
         (m_lastReceiveTimer.elapsed() > TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE));
}

void QnTransactionTransport::serializeAndSendNextDataBuffer()
{
    assert( !m_dataToSend.empty() );
    
    if( m_base64EncodeOutgoingTransactions )
        aggregateOutgoingTransactionsNonSafe();

    DataToSend& dataCtx = m_dataToSend.front();

    if( m_base64EncodeOutgoingTransactions )
        dataCtx.sourceData = dataCtx.sourceData.toBase64(); //TODO #ak should use streaming base64 encoder in addData method

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
                dataCtx.encodedSourceData = GZipCompressor::compressData( dataCtx.encodedSourceData );
            }
        }
        else    //m_peerRole == prOriginating
        {
            if( m_outgoingDataSocket )
            {
                //sending transactions as a POST request
                nx_http::Request request;
                request.requestLine.method = nx_http::Method::POST;
                const auto fullUrl = generatePostTranUrl();
                request.requestLine.url = fullUrl.path() + (fullUrl.hasQuery() ? (QLatin1String("?") + fullUrl.query()) : QString());;
                request.requestLine.version = nx_http::http_1_1;

                for( const auto& header: m_outgoingClientHeaders )
                    request.headers.emplace( header );

                //adding authorizationUrl
                if( !nx_http::AuthInfoCache::addAuthorizationHeader(
                        fullUrl,
                        &request,
                        m_httpAuthCacheItem ) )
                {
                    Q_ASSERT( false );
                }

                request.headers.emplace( "Date", dateTimeToHTTPFormat(QDateTime::currentDateTime()) );
                addHttpChunkExtensions( &request.headers );
                request.headers.emplace(
                    "Content-Length",
                    nx_http::BufferType::number((int)(dataCtx.sourceData.size())) );
                request.messageBody = dataCtx.sourceData;
                dataCtx.encodedSourceData = request.serialized();
            }
            else
            {
                dataCtx.encodedSourceData = dataCtx.sourceData;
            }
        }
    }
    using namespace std::placeholders;
    NX_LOG( lit("Sending data buffer (%1 bytes) to the peer %2").
        arg(dataCtx.encodedSourceData.size()).arg(m_remotePeer.id.toString()), cl_logDEBUG2 );

    if( m_outgoingDataSocket )
    {
        if( !m_outgoingDataSocket->sendAsync(
                dataCtx.encodedSourceData,
                std::bind( &QnTransactionTransport::onDataSent, this, _1, _2 ) ) )
        {
            const auto osErrorText = SystemError::getLastOSErrorText();
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error sending data to peer %1. %2. Disconnecting...").
                arg(m_remotePeer.id.toString()).arg(osErrorText), cl_logWARNING);
            setStateNoLock( State::Error );
        }
    }
    else  //m_peerRole == prOriginating
    {
        assert( m_peerRole == prOriginating && m_connectionType != ConnectionType::bidirectional );

        //using http client just to authenticate on server
        if( !m_outgoingTranClient )
        {
            m_outgoingTranClient = nx_http::AsyncHttpClient::create();
            m_outgoingTranClient->setSendTimeoutMs( TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE );
            m_outgoingTranClient->setResponseReadTimeoutMs( TCP_KEEPALIVE_TIMEOUT * KEEPALIVE_MISSES_BEFORE_CONNECTION_FAILURE );
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
                this, &QnTransactionTransport::postTransactionDone,
                Qt::DirectConnection );
            fillAuthInfo( m_outgoingTranClient, true );

            m_postTranBaseUrl = m_remoteAddr;
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

        if( !m_outgoingTranClient->doPost(
                generatePostTranUrl(),
                m_base64EncodeOutgoingTransactions
                    ? "application/text"
                    : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ),
                dataCtx.encodedSourceData ) )
        {
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("Failed to initiate POST transaction request to %1. %2").
                arg(m_outgoingTranClient->url().toString()).arg(SystemError::getLastOSErrorText()),
                cl_logWARNING );
            setStateNoLock( Error );
        }
    }
}

void QnTransactionTransport::onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent )
{
    QnMutexLocker lk( &m_mutex );

    if( errorCode )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Failed to send %1 bytes to %2. %3").arg(m_dataToSend.front().encodedSourceData.size()).
            arg(m_remotePeer.id.toString()).arg(SystemError::toString(errorCode)), cl_logWARNING );
        m_dataToSend.pop_front();
        return setStateNoLock( State::Error );
    }
    assert( bytesSent == (size_t)m_dataToSend.front().encodedSourceData.size() );

    m_dataToSend.pop_front();
    if( m_dataToSend.empty() )
        return;

    serializeAndSendNextDataBuffer();
}

void QnTransactionTransport::at_responseReceived(const nx_http::AsyncHttpClientPtr& client)
{
    const int statusCode = client->response()->statusLine.statusCode;

    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::at_responseReceived. statusCode = %1").
        arg(statusCode), cl_logDEBUG2 );

    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        if (m_authByKey) {
            m_authByKey = false;
            fillAuthInfo( m_httpClient, m_authByKey );
            QTimer::singleShot(0, this, SLOT(repeatDoGet()));
        }
        else {
            QnUuid guid(nx_http::getHeaderValue( client->response()->headers, Qn::EC2_SERVER_GUID_HEADER_NAME ));
            if (!guid.isNull()) {
                emit peerIdDiscovered(remoteAddr(), guid);
                emit remotePeerUnauthorized(guid);
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

    //checking remote server protocol version
    nx_http::HttpHeaders::const_iterator ec2ProtoVersionIter = 
        client->response()->headers.find(Qn::EC2_PROTO_VERSION_HEADER_NAME);
    const int remotePeerEcProtoVersion = ec2ProtoVersionIter == client->response()->headers.end()
        ? nx_ec::INITIAL_EC2_PROTO_VERSION
        : ec2ProtoVersionIter->second.toInt();
    if( nx_ec::EC2_PROTO_VERSION != remotePeerEcProtoVersion )
    {
        NX_LOG( QString::fromLatin1("Cannot connect to server %1 because of different EC2 proto version. "
            "Local peer version: %2, remote peer version: %3").
            arg(client->url().toString()).arg(nx_ec::EC2_PROTO_VERSION).arg(remotePeerEcProtoVersion),
            cl_logWARNING );
        cancelConnecting();
        return;
    }
    
    m_remotePeer.id = QnUuid(itrGuid->second);
    if (itrRuntimeGuid != client->response()->headers.end())
        m_remotePeer.instanceId = QnUuid(itrRuntimeGuid->second);

    Q_ASSERT(!m_remotePeer.instanceId.isNull());
    m_remotePeer.peerType = Qn::PT_Server; // outgoing connections for server peers only
    #ifdef USE_JSON
        m_remotePeer.dataFormat = Qn::JsonFormat;
    #else
        m_remotePeer.dataFormat = Qn::UbjsonFormat;
    #endif

    emit peerIdDiscovered(remoteAddr(), m_remotePeer.id);

    if( (statusCode/100) != (nx_http::StatusCode::ok/100) ) //checking that statusCode is 2xx
    {
        cancelConnecting();
        return;
    }

    if (getState() == QnTransactionTransport::Error || getState() == QnTransactionTransport::Closed) {
        return;
    }

    nx_http::HttpHeaders::const_iterator contentTypeIter = client->response()->headers.find("Content-Type");
    if( contentTypeIter == client->response()->headers.end() )
    {
        NX_LOG( lit("Remote transaction server (%1) did not specify Content-Type in response. Aborting connecion...")
            .arg(client->url().toString()), cl_logWARNING );
        cancelConnecting();
        return;
    }

    if( !m_multipartContentParser->setContentType( contentTypeIter->second ) )
    {
        NX_LOG( lit("Remote transaction server (%1) specified Content-Type (%2) which does not define multipart HTTP content")
            .arg(client->url().toString()).arg(QLatin1String(contentTypeIter->second)), cl_logWARNING );
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
            auto ungzip = std::make_shared<GZipUncompressor>();
            ungzip->setNextFilter( std::move(m_incomingTransactionStreamParser) );
            m_incomingTransactionStreamParser = std::move(ungzip);
        }
        else
        {
            //TODO #ak unsupported Content-Encoding ?
        }
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();

    if (getState() == ConnectingStage1) {
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(m_remotePeer.id, true);
        if (lockOK) {
            setState(ConnectingStage2);
            assert( data.isEmpty() );
        }
        else {
            QnMutexLocker lk( &m_mutex );
            QUrlQuery query = QUrlQuery(m_remoteAddr);
            query.addQueryItem("canceled", QString());
            m_remoteAddr.setQuery(query);
        }
        QTimer::singleShot(0, this, SLOT(repeatDoGet()));
    }
    else {
        if( nx_http::getHeaderValue(
                m_httpClient->response()->headers,
                Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME ) == "true" )
        {
            //inserting base64 decoder before the last filter
            m_incomingTransactionStreamParser = nx_bsf::insert(
                m_incomingTransactionStreamParser,
                nx_bsf::last( m_incomingTransactionStreamParser ),
                std::make_shared<Base64DecoderFilter>() );

            //base64-encoded data contains multiple transactions so
            //    inserting sized data decoder after base64 decoder
            m_incomingTransactionStreamParser = nx_bsf::insert(
                m_incomingTransactionStreamParser,
                nx_bsf::last( m_incomingTransactionStreamParser ),
                std::make_shared<nx_bsf::SizedDataDecodingFilter>() );
        }

        m_incomingDataSocket = m_httpClient->takeSocket();
        if( m_connectionType == ConnectionType::bidirectional )
        {
            m_outgoingDataSocket = m_incomingDataSocket;
            QnMutexLocker lk( &m_mutex );
            startSendKeepAliveTimerNonSafe();
        }
        else
        {
            QnMutexLocker lk( &m_mutex );
            startSendKeepAliveTimerNonSafe();
        }

        m_httpClient.reset();
        if (QnTransactionTransport::tryAcquireConnected(m_remotePeer.id, true)) {
            setExtraDataBuffer(data);
            m_peerRole = prOriginating;
            setState(QnTransactionTransport::Connected);
        }
        else {
            cancelConnecting();
        }
    }
}

void QnTransactionTransport::at_httpClientDone( const nx_http::AsyncHttpClientPtr& client )
{
    NX_LOG( QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::at_httpClientDone. state = %1").
        arg((int)client->state()), cl_logDEBUG2 );

    nx_http::AsyncHttpClient::State state = client->state();
    if( state == nx_http::AsyncHttpClient::sFailed ) {
        cancelConnecting();
    }
}

void QnTransactionTransport::processTransactionData(const QByteArray& data)
{
    Q_ASSERT( m_peerRole == prOriginating );
    if( !m_incomingTransactionStreamParser->processData( data ) )
    {
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error processing incoming data (4) from peer %1. Disconnecting...").
            arg(m_remotePeer.id.toString()), cl_logWARNING);
        return setStateNoLock( State::Error );
    }
}

bool QnTransactionTransport::isReadyToSend(ApiCommand::Value command) const
{
    if (m_state == ReadyForStreaming) {
        // allow to send system command immediately, without tranSyncRequest
        return ApiCommand::isSystem(command) ? true : m_writeSync;
    }
    else {
        return false;
    }
}

bool QnTransactionTransport::isReadSync(ApiCommand::Value command) const 
{
    if (m_state == ReadyForStreaming) {
        // allow to read system command immediately, without tranSyncRequest
        return ApiCommand::isSystem(command) ? true : m_readSync;
    }
    else {
        return false;
    }
}

QString QnTransactionTransport::toString( State state )
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

void QnTransactionTransport::addHttpChunkExtensions( nx_http::HttpHeaders* const headers )
{
    for( auto val: m_beforeSendingChunkHandlers )
        val.second( this, headers );
}

void QnTransactionTransport::processChunkExtensions( const nx_http::HttpHeaders& headers )
{
    if( headers.empty() )
        return;

    for( auto val: m_httpChunkExtensonHandlers )
        val.second( this, headers );
}

void QnTransactionTransport::setExtraDataBuffer(const QByteArray& data) 
{ 
    QnMutexLocker lk( &m_mutex );
    assert(m_extraData.isEmpty());
    m_extraData = data;
}

bool QnTransactionTransport::sendSerializedTransaction(Qn::SerializationFormat srcFormat, const QByteArray& serializedTran, const QnTransactionTransportHeader& _header) 
{
    if (srcFormat != m_remotePeer.dataFormat)
        return false;

    QnTransactionTransportHeader header(_header);
    assert(header.processedPeers.contains(m_localPeer.id));
    header.fillSequence();
    switch (m_remotePeer.dataFormat) {
    case Qn::JsonFormat:
        addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithoutHeader(serializedTran, header) + QByteArray("\r\n"));
        break;
    //case Qn::BnsFormat:
    //    addData(QnBinaryTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
        break;
    case Qn::UbjsonFormat: {

        if( QnLog::instance(QnLog::EC2_TRAN_LOG)->logLevel() >= cl_logDEBUG1 )
        {
            QnAbstractTransaction abtractTran;
            QnUbjsonReader<QByteArray> stream(&serializedTran);
            QnUbjson::deserialize(&stream, &abtractTran);
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("send direct transaction %1 to peer %2").arg(abtractTran.toString()).arg(remotePeer().id.toString()), cl_logDEBUG1 );
        }

        addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
        break;
    }
    default:
        qWarning() << "Client has requested data in the unsupported format" << m_remotePeer.dataFormat;
        addData(QnUbjsonTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
        break;
    }

    return true;
}

void QnTransactionTransport::setRemoteIdentityTime(qint64 time)
{
    m_remoteIdentityTime = time;
}

qint64 QnTransactionTransport::remoteIdentityTime() const
{
    return m_remoteIdentityTime;
}

bool QnTransactionTransport::skipTransactionForMobileClient(ApiCommand::Value command) {
    switch (command) {
    case ApiCommand::getMediaServersEx:
    case ApiCommand::saveCameras:
    case ApiCommand::getCamerasEx:
    case ApiCommand::getUsers:
    case ApiCommand::saveLayouts:
    case ApiCommand::getLayouts:
    case ApiCommand::saveResource:
    case ApiCommand::removeResource:
    case ApiCommand::removeCamera:
    case ApiCommand::removeMediaServer:
    case ApiCommand::removeUser:
    case ApiCommand::removeLayout:
    case ApiCommand::saveCamera:
    case ApiCommand::saveMediaServer:
    case ApiCommand::saveUser:
    case ApiCommand::saveLayout:
    case ApiCommand::setResourceStatus:
    case ApiCommand::setResourceParam:
    case ApiCommand::setResourceParams:
    case ApiCommand::saveCameraUserAttributes:
    case ApiCommand::saveServerUserAttributes:
    case ApiCommand::getCameraHistoryItems:
    case ApiCommand::addCameraHistoryItem:
        return false;
    default:
        break;
    }
    return true;
}

void QnTransactionTransport::scheduleAsyncRead()
{
    if( !m_incomingDataSocket )
        return;

    using namespace std::placeholders;
    if( m_incomingDataSocket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) ) )
    {
        m_asyncReadScheduled = true;
        m_lastReceiveTimer.restart();
    }
    else
    {
        const auto osErrorText = SystemError::getLastOSErrorText();
        NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error reading from peer %1. %2. Disconnecting...").
            arg(m_remotePeer.id.toString()).arg(osErrorText), cl_logWARNING);
        setStateNoLock( State::Error );
    }
}

void QnTransactionTransport::startListeningNonSafe()
{
    assert( m_incomingDataSocket || m_outgoingDataSocket );
    m_httpStreamReader.resetState();

    if( m_incomingDataSocket )
    {
        m_incomingDataSocket->setRecvTimeout(SOCKET_TIMEOUT);
        m_incomingDataSocket->setSendTimeout(SOCKET_TIMEOUT);
        m_incomingDataSocket->setNonBlockingMode(true);
        using namespace std::placeholders;
        m_lastReceiveTimer.restart();
        m_readBuffer.reserve( m_readBuffer.size() + DEFAULT_READ_BUFFER_SIZE );
        if( !m_incomingDataSocket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) ) )
        {
            const auto osErrorText = SystemError::getLastOSErrorText();
            NX_LOG(QnLog::EC2_TRAN_LOG, lit("Error reading incoming data from peer %1. %2. Disconnecting...").
                arg(m_remotePeer.id.toString()).arg(osErrorText), cl_logWARNING);
            m_lastReceiveTimer.invalidate();
            setStateNoLock( Error );
            return;
        }
    }
}

void QnTransactionTransport::postTransactionDone( const nx_http::AsyncHttpClientPtr& client )
{
    QnMutexLocker lk( &m_mutex );

    assert( client == m_outgoingTranClient );

    if( client->failed() || !client->response() )
    {
        NX_LOG( QnLog::EC2_TRAN_LOG, lit("Unknown network error posting transaction to %1").
            arg(m_postTranBaseUrl.toString()), cl_logWARNING );
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
        if( !m_outgoingTranClient->doPost(
                m_postTranBaseUrl,
                m_base64EncodeOutgoingTransactions
                    ? "application/text"
                    : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ),
                dataCtx.encodedSourceData ) )
        {
            NX_LOG( QnLog::EC2_TRAN_LOG, lit("Failed (2) to initiate POST transaction request to %1. %2").
                arg(m_postTranBaseUrl.toString()).arg(SystemError::getLastOSErrorText()), cl_logWARNING );
            setStateNoLock( Error );
            m_outgoingTranClient.reset();
        }
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

#ifdef PIPELINE_POST_REQUESTS
    //----------------------------------------------------------------------------------------
    //TODO #ak since http client does not support http pipelining we have to send 
        //POST requests directly from this class.
        //This block does it
    m_outgoingClientHeaders.clear();
    m_outgoingClientHeaders.emplace_back(
        "User-Agent",
        nx_http::userAgentString() );
    m_outgoingClientHeaders.emplace_back(
        "Content-Type",
        m_base64EncodeOutgoingTransactions
            ? "application/text"
            : Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ) );
    m_outgoingClientHeaders.emplace_back( "Host", m_remoteAddr.host().toLatin1() );
    m_outgoingClientHeaders.emplace_back(
        Qn::EC2_CONNECTION_GUID_HEADER_NAME,
        m_connectionGuid.toByteArray() );
    m_outgoingClientHeaders.emplace_back(
        Qn::EC2_CONNECTION_DIRECTION_HEADER_NAME,
        ConnectionType::toString(ConnectionType::outgoing) );
    if( m_base64EncodeOutgoingTransactions )    //informing server that transaction is encoded
        m_outgoingClientHeaders.emplace_back(
            Qn::EC2_BASE64_ENCODING_REQUIRED_HEADER_NAME,
            "true" );
    m_httpAuthCacheItem = client->authCacheItem();
    auto nxUsernameHeaderIter = client->request().headers.find( "X-Nx-User-Name" );
    if( nxUsernameHeaderIter != client->request().headers.end() )
        m_outgoingClientHeaders.emplace_back( *nxUsernameHeaderIter );

    assert( !m_outgoingDataSocket );
    m_outgoingDataSocket = client->takeSocket();
    m_outgoingTranClient.reset();

    using namespace std::placeholders;
    //monitoring m_outgoingDataSocket for connection close
    m_dummyReadBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_outgoingDataSocket->readSomeAsync(
        &m_dummyReadBuffer,
        std::bind(&QnTransactionTransport::monitorConnectionForClosure, this, _1, _2) );
    //----------------------------------------------------------------------------------------
#endif //PIPELINE_POST_REQUESTS

    m_dataToSend.pop_front();
    if( m_dataToSend.empty() )
        return;

    serializeAndSendNextDataBuffer();
}

void QnTransactionTransport::setBeforeDestroyCallback(std::function<void ()> ttFinishCallback)
{
    m_ttFinishCallback = ttFinishCallback;
}

}
