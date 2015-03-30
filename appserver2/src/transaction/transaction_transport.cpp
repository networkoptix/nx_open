#include "transaction_transport.h"

#include <atomic>

#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include <nx_ec/ec_proto_version.h>
#include <utils/media/custom_output_stream.h>

#include "transaction_message_bus.h"
#include "utils/common/log.h"
#include "utils/common/systemerror.h"
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


/*!
    Real transaction posted to the QnTransactionMessageBus can be greater 
    (all transactions that read with single read from socket)
*/
static const int MAX_TRANS_TO_POST_AT_A_TIME = 16;

//#define SEND_4BYTE_TRANSACTION_SIZE

namespace ec2
{

static const int DEFAULT_READ_BUFFER_SIZE = 4 * 1024;
static const int SOCKET_TIMEOUT = 1000 * 1000;
static const int TCP_KEEPALIVE_TIMEOUT = 1000 * 5;
const char* QnTransactionTransport::TUNNEL_MULTIPART_BOUNDARY = "ec2boundary";
const char* QnTransactionTransport::TUNNEL_CONTENT_TYPE = "multipart/mixed; boundary=ec2boundary";

QSet<QnUuid> QnTransactionTransport::m_existConn;
QnTransactionTransport::ConnectingInfoMap QnTransactionTransport::m_connectingConn;
QMutex QnTransactionTransport::m_staticMutex;

QnTransactionTransport::QnTransactionTransport(
    const ApiPeerData &localPeer,
    const QSharedPointer<AbstractStreamSocket>& socket )
:
    m_localPeer(localPeer),
    m_lastConnectTime(0), 
    m_readSync(false), 
    m_writeSync(false),
    m_syncDone(false),
    m_socket(socket),
    m_state(NotDefined), 
    m_chunkHeaderLen(0),
    m_chunkLen(0), 
    m_sendOffset(0), 
    m_connected(false),
    m_prevGivenHandlerID(0),
    m_postedTranCount(0),
    m_asyncReadScheduled(false),
    m_remoteIdentityTime(0),
    m_incomingConnection(socket),
    m_incomingTunnelOpened(false)
{
    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_lastReceiveTimer.invalidate();
    m_emptyChunkData = QnChunkedTransferEncoder::serializedTransaction(QByteArray(), std::vector<nx_http::ChunkExtension>());

    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    auto processTranFunc = std::bind(
        &QnTransactionTransport::receivedTransaction,
        this,
        std::placeholders::_1 );
    m_contentParser.setNextFilter( std::make_shared<CustomOutputStream<decltype(processTranFunc)> >(std::move(processTranFunc)) );
}


QnTransactionTransport::~QnTransactionTransport()
{
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("~QnTransactionTransport for object = %1").arg((size_t) this,  0, 16), cl_logDEBUG1);

    if( m_httpClient )
        m_httpClient->terminate();

    {
        QMutexLocker lk( &m_mutex );
        m_state = Closed;
    }
    closeSocket();
    //not calling QnTransactionTransport::close since it will emit stateChanged, 
        //which can potentially lead to some trouble

    if (m_connected)
        connectDone(m_remotePeer.id);
}

void QnTransactionTransport::addData(QByteArray&& data)
{
    QMutexLocker lock(&m_mutex);
    m_dataToSend.push_back( std::move( data ) );
    if( (m_dataToSend.size() == 1) && m_socket )
        serializeAndSendNextDataBuffer();
}

void QnTransactionTransport::addEncodedData(QByteArray&& data)
{
    QMutexLocker lock(&m_mutex);
    DataToSend dataToSend;
    dataToSend.encodedSourceData = std::move( data );
    m_dataToSend.push_back( std::move( dataToSend ) );
    if( (m_dataToSend.size() == 1) && m_socket )
        serializeAndSendNextDataBuffer();
}

int QnTransactionTransport::readChunkHeader(const quint8* data, int dataLen, nx_http::ChunkHeader* const chunkHeader)
{
    const int bytesRead = chunkHeader->parse( QByteArray::fromRawData(reinterpret_cast<const char*>(data), dataLen) );
    return bytesRead  == -1 
        ? 0   //parse error
        : bytesRead;
}

void QnTransactionTransport::closeSocket()
{
    if (m_socket) {
        m_socket->terminateAsyncIO( true );
        m_socket->close();
        m_socket.reset();
    }
}

void QnTransactionTransport::setState(State state)
{
    QMutexLocker lock(&m_mutex);
    setStateNoLock(state);
}

void QnTransactionTransport::processExtraData()
{
    QMutexLocker lock(&m_mutex);
    processTransactionData(m_extraData);
    m_extraData.clear();
}

void QnTransactionTransport::startListening()
{
    QMutexLocker lock(&m_mutex);

    assert( m_socket );
    m_httpStreamReader.resetState();

    if( m_socket )
    {
        m_socket->setRecvTimeout(SOCKET_TIMEOUT);
        m_socket->setSendTimeout(SOCKET_TIMEOUT);
        m_socket->setNonBlockingMode(true);
        m_chunkHeaderLen = 0;
        using namespace std::placeholders;
        m_lastReceiveTimer.restart();
        if( !m_socket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) ) )
        {
            m_lastReceiveTimer.invalidate();
            setStateNoLock( Error );
            return;
        }
        if( m_remotePeer.isServer() )
            if( !m_socket->registerTimer( TCP_KEEPALIVE_TIMEOUT, std::bind(&QnTransactionTransport::sendHttpKeepAlive, this) ) )
            {
                m_lastReceiveTimer.invalidate();
                setStateNoLock( Error );
                return;
            }
    }
}

void QnTransactionTransport::setStateNoLock(State state)
{
    if (state == Connected) {
        m_connected = true;
    }
    else if (state == Error) {
    }
    else if (state == ReadyForStreaming) {
    }
    if (this->m_state != state) {
        this->m_state = state;
        emit stateChanged(state);
    }
}

QnTransactionTransport::State QnTransactionTransport::getState() const
{
    QMutexLocker lock(&m_mutex);
    return m_state;
}

int QnTransactionTransport::setHttpChunkExtensonHandler( HttpChunkExtensonHandler eventHandler )
{
    QMutexLocker lk(&m_mutex);
    m_httpChunkExtensonHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

int QnTransactionTransport::setBeforeSendingChunkHandler( BeforeSendingChunkHandler eventHandler )
{
    QMutexLocker lk(&m_mutex);
    m_beforeSendingChunkHandlers.emplace( ++m_prevGivenHandlerID, std::move(eventHandler) );
    return m_prevGivenHandlerID;
}

void QnTransactionTransport::removeEventHandler( int eventHandlerID )
{
    QMutexLocker lk(&m_mutex);
    m_httpChunkExtensonHandlers.erase( eventHandlerID );
    m_beforeSendingChunkHandlers.erase( eventHandlerID );
}

AbstractStreamSocket* QnTransactionTransport::getSocket() const
{
    return m_socket.data();
}

void QnTransactionTransport::close()
{
    setState(State::Closed);    //changing state before freeing socket so that everyone 
                                //stop using socket before it is actually freed
 
    closeSocket();
    {
        QMutexLocker lock(&m_mutex);
        assert( !m_socket );
        m_readSync = false;
        m_writeSync = false;
    }
}

void QnTransactionTransport::fillAuthInfo()
{
    if (!QnAppServerConnectionFactory::videowallGuid().isNull()) {
        m_httpClient->addRequestHeader("X-NetworkOptix-VideoWall", QnAppServerConnectionFactory::videowallGuid().toString().toUtf8());
        return;
    }

    QnMediaServerResourcePtr ownServer = qnResPool->getResourceById(qnCommon->moduleGUID()).dynamicCast<QnMediaServerResource>();
    if (ownServer && m_authByKey) 
    {
        m_httpClient->setUserName(ownServer->getId().toString().toLower());    
        m_httpClient->setUserPassword(ownServer->getAuthKey());
    }
    else {
        QUrl url = QnAppServerConnectionFactory::url();
        m_httpClient->setUserName(url.userName());
		if (dbManager) {
	        QnUserResourcePtr adminUser = QnGlobalSettings::instance()->getAdminUser();
	        if (adminUser) {
	            m_httpClient->setUserPassword(adminUser->getDigest());
	            m_httpClient->setAuthType(nx_http::AsyncHttpClient::authDigestWithPasswordHash);
	        }
        }
        else {
            m_httpClient->setUserPassword(url.password());
        }
    }
}

void QnTransactionTransport::doOutgoingConnect(QUrl remoteAddr)
{
    setState(ConnectingStage1);
    m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    m_httpClient->setDecodeChunkedMessageBody( false ); //chunked decoding is done in this class
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnTransactionTransport::at_responseReceived, Qt::DirectConnection);
    //connect(m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable, this, &QnTransactionTransport::at_responseReceived, Qt::DirectConnection);
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnTransactionTransport::at_httpClientDone, Qt::DirectConnection);
    
    fillAuthInfo();
    if( m_localPeer.isServer() )
        m_httpClient->addRequestHeader(
            nx_ec::EC2_SYSTEM_NAME_HEADER_NAME,
            QnCommonModule::instance()->localSystemName().toUtf8() );

    if (!remoteAddr.userName().isEmpty())
    {
        remoteAddr.setUserName(QString());
        remoteAddr.setPassword(QString());
    }

    QUrlQuery q = QUrlQuery(remoteAddr.query());

    // Client reconnects to the server
    if( m_localPeer.isClient() ) {
        q.removeQueryItem("isClient");
        q.addQueryItem("isClient", QString());
        setState(ConnectingStage2); // one GET method for client peer is enough
        setReadSync(true);
    }

    remoteAddr.setQuery(q);
    m_remoteAddr = remoteAddr;
    m_httpClient->removeAdditionalHeader( nx_ec::EC2_CONNECTION_STATE_HEADER_NAME );
    m_httpClient->addRequestHeader( nx_ec::EC2_CONNECTION_STATE_HEADER_NAME, toString(getState()).toLatin1() );
    if (!m_httpClient->doGet(remoteAddr)) {
        qWarning() << Q_FUNC_INFO << "Failed to execute m_httpClient->doGet. Reconnect transaction transport";
        setState(Error);
    }
}

bool QnTransactionTransport::tryAcquireConnecting(const QnUuid& remoteGuid, bool isOriginator)
{
    QMutexLocker lock(&m_staticMutex);

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
    QMutexLocker lock(&m_staticMutex);
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
    QMutexLocker lock(&m_staticMutex);
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
    QMutexLocker lock(&m_staticMutex);
    m_existConn.remove(id);
}

void QnTransactionTransport::repeatDoGet()
{
    m_httpClient->removeAdditionalHeader( nx_ec::EC2_CONNECTION_STATE_HEADER_NAME );
    m_httpClient->addRequestHeader( nx_ec::EC2_CONNECTION_STATE_HEADER_NAME, toString(getState()).toLatin1() );
    if (!m_httpClient->doGet(m_remoteAddr))
        cancelConnecting();
}

void QnTransactionTransport::cancelConnecting()
{
    if (getState() == ConnectingStage2)
        QnTransactionTransport::connectingCanceled(m_remotePeer.id, true);
    NX_LOG( lit("%1 Connection canceled from state %2").arg(Q_FUNC_INFO).arg(toString(getState())), cl_logWARNING );
    setState(Error);
}

void QnTransactionTransport::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    QMutexLocker lock(&m_mutex);

    m_asyncReadScheduled = false;
    m_lastReceiveTimer.invalidate();

    if( errorCode || bytesRead == 0 )   //error or connection closed
    {
        if( errorCode == SystemError::timedOut )
        {
            NX_LOG( lit("Peer %1 timed out. Disconnecting...").arg(m_remotePeer.id.toString()), cl_logWARNING );
        }
        return setStateNoLock( State::Error );
    }

    if (m_state >= QnTransactionTransport::Closed)
        return;

    assert( m_state == ReadyForStreaming );

    //if incoming connection then expecting POST request to open incoming tunnel
    if( m_incomingConnection && !m_incomingTunnelOpened )
    {
        if( !readCreateIncomingTunnelMessage() )
        {
            NX_LOG( lit("Error parsing open tunnel request from peer %1. Disconnecting...").
                arg(m_remotePeer.id.toString()), cl_logWARNING );
            return setStateNoLock( State::Error );
        }
        if( !m_incomingTunnelOpened )
        {
            //reading futher
            scheduleAsyncRead();
            return;
        }
    }

    //parsing and processing input data
    m_contentParser.processData( m_readBuffer );
    m_readBuffer.resize(0);

    if( m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME )
        return; //not reading futher while that much transactions are not processed yet

    scheduleAsyncRead();
}

void QnTransactionTransport::receivedTransaction( const QnByteArrayConstRef& tranDataWithHeader )
{
    //calling processChunkExtensions
    processChunkExtensions( m_contentParser.prevFrameHeaders() );

#ifdef SEND_4BYTE_TRANSACTION_SIZE
    //skipping transaction size
    const QnByteArrayConstRef& tranData = tranDataWithHeader.mid( sizeof(uint32_t) );
#else
    const QnByteArrayConstRef& tranData = tranDataWithHeader;
#endif

    QByteArray serializedTran;
    QnTransactionTransportHeader transportHeader;
    if( !QnUbjsonTransactionSerializer::deserializeTran(
            reinterpret_cast<const quint8*>(tranData.constData()),
            tranData.size(),
            transportHeader,
            serializedTran ) )
    {
        Q_ASSERT( false );
        return;
    }
    Q_ASSERT( !transportHeader.processedPeers.empty() );
    NX_LOG(QnLog::EC2_TRAN_LOG, lit("QnTransactionTransport::receivedTransaction. Got transaction with seq %1 from %2").
        arg(transportHeader.sequence).arg(m_remotePeer.id.toString()), cl_logDEBUG1);
    emit gotTransaction(serializedTran, transportHeader);
    ++m_postedTranCount;
}

bool QnTransactionTransport::hasUnsendData() const
{
    QMutexLocker lock(&m_mutex);
    return !m_dataToSend.empty();
}
 
void QnTransactionTransport::transactionProcessed()
{
    QMutexLocker lock(&m_mutex);

    --m_postedTranCount;
    if( m_postedTranCount >= MAX_TRANS_TO_POST_AT_A_TIME ||     //not reading futher while that much transactions are not processed yet
        m_asyncReadScheduled ||      //async read is ongoing already, overlapping reads are not supported by sockets api
        m_state > ReadyForStreaming )
    {
        return;
    }

    assert( m_socket );

    scheduleAsyncRead();
}

void QnTransactionTransport::sendHttpKeepAlive()
{
    QMutexLocker lock(&m_mutex);
    if (m_dataToSend.empty()) 
    {
        m_dataToSend.push_back( QByteArray() );
        m_dataToSend.front().encodedSourceData = m_emptyChunkData;
        serializeAndSendNextDataBuffer();
    }
    if( !m_socket->registerTimer( TCP_KEEPALIVE_TIMEOUT, std::bind(&QnTransactionTransport::sendHttpKeepAlive, this) ) )
        setStateNoLock( State::Error );
}

bool QnTransactionTransport::isHttpKeepAliveTimeout() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastReceiveTimer.isValid() &&  //if not valid we still have not begun receiving transactions
        (m_lastReceiveTimer.elapsed() > TCP_KEEPALIVE_TIMEOUT * 3);
}

void QnTransactionTransport::serializeAndSendNextDataBuffer()
{
    assert( !m_dataToSend.empty() );
    DataToSend& dataCtx = m_dataToSend.front();
    if( dataCtx.encodedSourceData.isEmpty() )
    {
#ifdef SEND_4BYTE_TRANSACTION_SIZE
        const uint32_t tranSize = htonl(dataCtx.sourceData.size());
#endif

        nx_http::HttpHeaders headers;
        headers.emplace( "Content-Type", Qn::serializationFormatToHttpContentType( m_remotePeer.dataFormat ) );
        headers.emplace( "Content-Length",
            nx_http::BufferType::number((int)(dataCtx.sourceData.size()
#ifdef SEND_4BYTE_TRANSACTION_SIZE
            + sizeof(tranSize)
#endif
            )) );
        addHttpChunkExtensions( &headers );

        dataCtx.encodedSourceData.clear();
        dataCtx.encodedSourceData += QByteArray("--")+TUNNEL_MULTIPART_BOUNDARY+"\r\n"; //TODO #ak move to some variable
        nx_http::serializeHeaders( headers, &dataCtx.encodedSourceData );
        dataCtx.encodedSourceData += "\r\n";
#ifdef SEND_4BYTE_TRANSACTION_SIZE
        dataCtx.encodedSourceData += QByteArray::fromRawData( reinterpret_cast<const char*>(&tranSize), sizeof(tranSize) );
#endif
        dataCtx.encodedSourceData += dataCtx.sourceData;
    }
    using namespace std::placeholders;
    assert( !dataCtx.encodedSourceData.isEmpty() );
    NX_LOG( lit("Sending data buffer (%1 bytes) to the peer %2").
        arg(dataCtx.encodedSourceData.size()).arg(m_remotePeer.id.toString()), cl_logDEBUG2 );
    if( !m_socket->sendAsync( dataCtx.encodedSourceData, std::bind( &QnTransactionTransport::onDataSent, this, _1, _2 ) ) )
        return setStateNoLock( State::Error );
}

void QnTransactionTransport::onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent )
{
    QMutexLocker lk( &m_mutex );

    if( errorCode )
    {
        NX_LOG( lit("Failed to send %1 bytes to %2. %3").arg(m_dataToSend.front().encodedSourceData.size()).
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
    int statusCode = client->response()->statusLine.statusCode;
    if (statusCode == nx_http::StatusCode::unauthorized)
    {
        if (m_authByKey) {
            m_authByKey = false;
            fillAuthInfo();
            QTimer::singleShot(0, this, SLOT(repeatDoGet()));
        }
        else {
            QnUuid guid(nx_http::getHeaderValue( client->response()->headers, "x-server-guid" ));
            if (!guid.isNull()) {
                emit peerIdDiscovered(m_remoteAddr, guid);
                emit remotePeerUnauthorized(guid);
            }
            cancelConnecting();
        }
        return;
    }


    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find("guid");
    nx_http::HttpHeaders::const_iterator itrRuntimeGuid = client->response()->headers.find("runtime-guid");
    nx_http::HttpHeaders::const_iterator itrSystemIdentityTime = client->response()->headers.find("system-identity-time");
    if (itrSystemIdentityTime != client->response()->headers.end())
        setRemoteIdentityTime(itrSystemIdentityTime->second.toLongLong());

    if (itrGuid == client->response()->headers.end())
    {
        cancelConnecting();
        return;
    }

    //checking remote server protocol version
    nx_http::HttpHeaders::const_iterator ec2ProtoVersionIter = 
        client->response()->headers.find(nx_ec::EC2_PROTO_VERSION_HEADER_NAME);
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
    emit peerIdDiscovered(m_remoteAddr, m_remotePeer.id);

    if (statusCode != nx_http::StatusCode::ok)
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

    if( !m_contentParser.setContentType( contentTypeIter->second ) )
    {
        NX_LOG( lit("Remote transaction server (%1) specified Content-Type (%2) which does not define multipart HTTP content")
            .arg(client->url().toString()).arg(QLatin1String(contentTypeIter->second)), cl_logWARNING );
        cancelConnecting();
        return;
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();

    if (getState() == ConnectingStage1) {
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(m_remotePeer.id, true);
        if (lockOK) {
            setState(ConnectingStage2);
            assert( data.isEmpty() );
        }
        else {
            QUrlQuery query = QUrlQuery(m_remoteAddr);
            query.addQueryItem("canceled", QString());
            m_remoteAddr.setQuery(query);
        }
        QTimer::singleShot(0, this, SLOT(repeatDoGet()));
    }
    else {
        m_socket = m_httpClient->takeSocket();
        //m_socket->setNoDelay( true );
        m_httpClient.reset();
        if (QnTransactionTransport::tryAcquireConnected(m_remotePeer.id, true)) {
            setExtraDataBuffer(data);

            //opening forward tunnel: sending POST with infinite body
            nx_http::Request request;
            request.requestLine.method = nx_http::Method::POST;
            request.requestLine.url = client->url();
            request.requestLine.version = nx_http::http_1_1;
            request.headers.emplace( "User-Agent", QN_ORGANIZATION_NAME " " QN_PRODUCT_NAME " " QN_APPLICATION_VERSION );
            request.headers.emplace( "Content-Type", TUNNEL_CONTENT_TYPE );
            request.headers.emplace( "Host", request.requestLine.url.host().toLatin1() );
            addEncodedData( request.serialized() );

            setState(QnTransactionTransport::Connected);
        }
        else {
            cancelConnecting();
        }
    }
}

void QnTransactionTransport::at_httpClientDone( const nx_http::AsyncHttpClientPtr& client )
{
    nx_http::AsyncHttpClient::State state = client->state();
    if( state == nx_http::AsyncHttpClient::sFailed ) {
        cancelConnecting();
    }
}

void QnTransactionTransport::processTransactionData(const QByteArray& data)
{
    m_contentParser.processData( data );
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
    QMutexLocker lk( &m_mutex );
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
        addData(QnJsonTransactionSerializer::instance()->serializedTransactionWithHeader(serializedTran, header));
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
    using namespace std::placeholders;
    if( m_socket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) ) )
    {
        m_asyncReadScheduled = true;
        m_lastReceiveTimer.restart();
    }
    else
    {
        setStateNoLock( State::Error );
    }
}

bool QnTransactionTransport::readCreateIncomingTunnelMessage()
{
    m_httpStreamReader.setBreakAfterReadingHeaders( true );

    Q_ASSERT( !m_incomingTunnelOpened );

    size_t bytesRead = 0;
    if( !m_httpStreamReader.parseBytes(
            m_readBuffer,
            m_readBuffer.size(),
            &bytesRead ) )
    {
        return false;
    }
    m_readBuffer.remove( 0, bytesRead );

    switch( m_httpStreamReader.state() )
    {
        case nx_http::HttpStreamReader::waitingMessageStart:
        case nx_http::HttpStreamReader::readingMessageHeaders:
        case nx_http::HttpStreamReader::pullingLineEndingBeforeMessageBody:
            break;  //reading futher

        case nx_http::HttpStreamReader::parseError:
            return false;

        case nx_http::HttpStreamReader::readingMessageBody:
        case nx_http::HttpStreamReader::messageDone:
            //read request and trailing CRLF
            if( m_httpStreamReader.message().type != nx_http::MessageType::request )
                return false;
            if( m_httpStreamReader.message().request->requestLine.method != nx_http::Method::POST )
                return false;
            if( !m_contentParser.setContentType(
                    nx_http::getHeaderValue(
                        m_httpStreamReader.message().request->headers, "Content-Type" ) ) )
            {
                return false;
            }
            m_incomingTunnelOpened = true;
            break;
    }

    return true;
}

}
