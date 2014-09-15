#include "transaction_transport.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

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


namespace ec2
{

static const int DEFAULT_READ_BUFFER_SIZE = 4 * 1024;
static const int SOCKET_TIMEOUT = 1000 * 1000;
static const int TCP_KEEPALIVE_TIMEOUT = 1000 * 5;

QSet<QUuid> QnTransactionTransport::m_existConn;
QnTransactionTransport::ConnectingInfoMap QnTransactionTransport::m_connectingConn;
QMutex QnTransactionTransport::m_staticMutex;

QnTransactionTransport::QnTransactionTransport(const ApiPeerData &localPeer, const QSharedPointer<AbstractStreamSocket>& socket):
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
    m_prevGivenHandlerID(0)
{
    m_readBuffer.reserve( DEFAULT_READ_BUFFER_SIZE );
    m_sendTimer.restart();
    m_lastReceiveTimer.restart();
    m_emptyChunkData = QnChunkedTransferEncoder::serializedTransaction(QByteArray(), std::vector<nx_http::ChunkExtension>());
}


QnTransactionTransport::~QnTransactionTransport()
{
    if( m_httpClient )
        m_httpClient->terminate();

    closeSocket();

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
        m_socket->cancelAsyncIO( aio::etRead );
        m_socket->cancelAsyncIO( aio::etWrite );
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
    processTransactionData(m_extraData);
    m_extraData.clear();
}

void QnTransactionTransport::startListening()
{
    if (m_socket) {
        m_socket->setRecvTimeout(SOCKET_TIMEOUT);
        m_socket->setSendTimeout(SOCKET_TIMEOUT);
        m_socket->setNonBlockingMode(true);
        m_chunkHeaderLen = 0;
        using namespace std::placeholders;
        m_socket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) );
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
    closeSocket();
    {
        QMutexLocker lock(&m_mutex);
        assert( !m_socket );
        m_readSync = false;
        m_writeSync = false;
    }
    setState(State::Closed);
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
    if (!m_httpClient->doGet(remoteAddr)) {
        qWarning() << Q_FUNC_INFO << "Failed to execute m_httpClient->doGet. Reconnect transaction transport";
        setState(Error);
    }
}

bool QnTransactionTransport::tryAcquireConnecting(const QUuid& remoteGuid, bool isOriginator)
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


void QnTransactionTransport::connectingCanceled(const QUuid& remoteGuid, bool isOriginator)
{
    QMutexLocker lock(&m_staticMutex);
    connectingCanceledNoLock(remoteGuid, isOriginator);
}

void QnTransactionTransport::connectingCanceledNoLock(const QUuid& remoteGuid, bool isOriginator)
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

bool QnTransactionTransport::tryAcquireConnected(const QUuid& remoteGuid, bool isOriginator)
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

void QnTransactionTransport::connectDone(const QUuid& id)
{
    QMutexLocker lock(&m_staticMutex);
    m_existConn.remove(id);
}

void QnTransactionTransport::repeatDoGet()
{
    if (!m_httpClient->doGet(m_remoteAddr))
        cancelConnecting();
}

void QnTransactionTransport::cancelConnecting()
{
    if (getState() == ConnectingStage2)
        QnTransactionTransport::connectingCanceled(m_remotePeer.id, true);
    qWarning() << Q_FUNC_INFO << "Connection canceled from state " << toString(getState());
    setState(Error);
}

void QnTransactionTransport::onSomeBytesRead( SystemError::ErrorCode errorCode, size_t bytesRead )
{
    QMutexLocker lock(&m_mutex);

    m_lastReceiveTimer.restart();

    if( errorCode || bytesRead == 0 )   //error or connection closed
        return setStateNoLock( State::Error );

    if (m_state == Error)
        return;

    assert( m_state == ReadyForStreaming );

    //TODO #ak it makes sense to use here some chunk parser class. At this moment http chunk parsing logic is implemented 
    //3 times in different parts of our code (async http client, sync http server and here)
    for( size_t readBufPos = 0;; )
    {
        //processing available data: we can have zero or more chunks in m_readBuffer
        if( m_chunkHeaderLen == 0 )  //chunk header has not been read yet
        {
            nx_http::ChunkHeader httpChunkHeader;
            //reading chunk header
            m_chunkHeaderLen = readChunkHeader(
                reinterpret_cast<const quint8*>(m_readBuffer.constData()) + readBufPos,
                m_readBuffer.size() - readBufPos,
                &httpChunkHeader );
            if( m_chunkHeaderLen == 0 )
            {
                if( readBufPos > 0 )
                    m_readBuffer.remove( 0, readBufPos );  //pop_front(readBufPos)
                break;  //not enough data in m_readBuffer to read http chunk header
            }
            m_chunkLen = httpChunkHeader.chunkSize;
            processChunkExtensions( httpChunkHeader );  //processing chunk extensions even before receiving whole transaction
        }

        assert( m_chunkHeaderLen > 0 );

        //have just read http chunk
        const size_t fullChunkSize = m_chunkHeaderLen + m_chunkLen + sizeof( "\r\n" ) - 1;
        if( (size_t)m_readBuffer.capacity() < fullChunkSize )
            m_readBuffer.reserve( fullChunkSize );

        if( (m_readBuffer.size() - readBufPos) < fullChunkSize )
        {
            //not enough data in m_readBuffer
            if( readBufPos > 0 )
                m_readBuffer.remove( 0, readBufPos );  //pop_front(readBufPos)
            break;
        }

        int payloadLen = m_chunkLen - 4;
        if (payloadLen > 0) {
            QByteArray serializedTran;
            QnTransactionTransportHeader transportHeader;
            if( !QnUbjsonTransactionSerializer::deserializeTran(
                    reinterpret_cast<const quint8*>(m_readBuffer.constData()) + readBufPos + m_chunkHeaderLen + 4,
                    m_chunkLen - 4,
                    transportHeader,
                    serializedTran ) )
            {
                assert( false );
            }
            assert( !transportHeader.processedPeers.empty() );
            //NX_LOG(lit("QnTransactionTransport::onSomeBytesRead. Got transaction with seq %1 from %2").arg(transportHeader.sequence).arg(m_remotePeer.id.toString()), cl_logDEBUG1);
            emit gotTransaction(serializedTran, transportHeader);
        }
        readBufPos += fullChunkSize;
        m_chunkHeaderLen = 0;

        if( readBufPos == (size_t)m_readBuffer.size() )
        {
            m_readBuffer.resize(0);
            break;  //processed all data
        }
    }

    using namespace std::placeholders;
    m_socket->readSomeAsync( &m_readBuffer, std::bind( &QnTransactionTransport::onSomeBytesRead, this, _1, _2 ) );
}

bool QnTransactionTransport::hasUnsendData() const
{
    QMutexLocker lock(&m_mutex);
    return !m_dataToSend.empty();
}

void QnTransactionTransport::sendHttpKeepAlive()
{
    QMutexLocker lock(&m_mutex);
    if (m_sendTimer.elapsed() > TCP_KEEPALIVE_TIMEOUT && m_dataToSend.empty() && m_socket) 
    {
        m_dataToSend.push_back( QByteArray() );
        m_dataToSend.front().encodedSourceData = m_emptyChunkData;
        serializeAndSendNextDataBuffer();
    }
}

bool QnTransactionTransport::isHttpKeepAliveTimeout() const
{
    QMutexLocker lock(&m_mutex);
    return m_lastReceiveTimer.elapsed() > TCP_KEEPALIVE_TIMEOUT * 2;
}

void QnTransactionTransport::serializeAndSendNextDataBuffer()
{
    assert( !m_dataToSend.empty() );
    m_sendTimer.restart();
    DataToSend& dataCtx = m_dataToSend.front();
    if( dataCtx.encodedSourceData.isEmpty() )
    {
        std::vector<nx_http::ChunkExtension> chunkExtensions;
        addHttpChunkExtensions( &chunkExtensions );
        dataCtx.encodedSourceData = QnChunkedTransferEncoder::serializedTransaction(
            dataCtx.sourceData,
            chunkExtensions );
    }

    using namespace std::placeholders;
    m_socket->sendAsync( dataCtx.encodedSourceData, std::bind( &QnTransactionTransport::onDataSent, this, _1, _2 ) );
}

void QnTransactionTransport::onDataSent( SystemError::ErrorCode errorCode, size_t bytesSent )
{
    QMutexLocker lk( &m_mutex );

    if( errorCode )
        return setStateNoLock( State::Error );
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
            QUuid guid(nx_http::getHeaderValue( client->response()->headers, "x-server-guid" ));
            if (!guid.isNull()) {
                emit peerIdDiscovered(m_remoteAddr, guid);
                emit remotePeerUnauthorized(guid);
            }
            cancelConnecting();
        }
        return;
    }


    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find("guid");

    if (itrGuid == client->response()->headers.end())
    {
        cancelConnecting();
        return;
    }

    m_remotePeer.id = QUuid(itrGuid->second);
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
    m_lastReceiveTimer.restart();
    m_chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.constData();
    size_t bufferLen = (size_t)data.size();
    nx_http::ChunkHeader httpChunkHeader;
    m_chunkHeaderLen = readChunkHeader(buffer, bufferLen, &httpChunkHeader);
    m_chunkLen = httpChunkHeader.chunkSize;
    while (m_chunkHeaderLen > 0) 
    {
        processChunkExtensions( httpChunkHeader );

        const size_t fullChunkLen = m_chunkHeaderLen + m_chunkLen + sizeof("\r\n")-1;
        if (bufferLen >= fullChunkLen)
        {
            int payloadLen = m_chunkLen - 4;
            if (payloadLen > 0) {
                QByteArray serializedTran;
                QnTransactionTransportHeader transportHeader;
                QnUbjsonTransactionSerializer::deserializeTran(buffer + m_chunkHeaderLen + 4, m_chunkLen - 4, transportHeader, serializedTran);
                assert( !transportHeader.processedPeers.empty() );
                NX_LOG(lit("QnTransactionTransport::processTransactionData. Got transaction with seq %1 from %2").arg(transportHeader.sequence).arg(m_remotePeer.id.toString()), cl_logDEBUG1);
                emit gotTransaction(serializedTran, transportHeader);
            }
            

            buffer += fullChunkLen;
            bufferLen -= fullChunkLen;
        }
        else {
            break;
        }
        httpChunkHeader.clear();
        m_chunkHeaderLen = readChunkHeader(buffer, bufferLen, &httpChunkHeader);
        m_chunkLen = httpChunkHeader.chunkSize;
    }

    if (bufferLen > 0) {
        if( (size_t)m_readBuffer.size() < bufferLen )
            m_readBuffer.resize(bufferLen);
        memcpy(m_readBuffer.data(), buffer, bufferLen);
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

void QnTransactionTransport::addHttpChunkExtensions( std::vector<nx_http::ChunkExtension>* const chunkExtensions )
{
    for( auto val: m_beforeSendingChunkHandlers )
        val.second( this, chunkExtensions );
}

void QnTransactionTransport::processChunkExtensions( const nx_http::ChunkHeader& httpChunkHeader )
{
    if( httpChunkHeader.extensions.empty() )
        return;

    for( auto val: m_httpChunkExtensonHandlers )
        val.second( this, httpChunkHeader.extensions );
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

#ifdef TRANSACTION_MESSAGE_BUS_DEBUG
        QnAbstractTransaction abtractTran;
        QnUbjsonReader<QByteArray> stream(&serializedTran);
        QnUbjson::deserialize(&stream, &abtractTran);
        qDebug() << "send direct transaction to peer " << remotePeer().id << "command=" << ApiCommand::toString(abtractTran.command) 
            << "tt seq=" << header.sequence << "db seq=" << abtractTran.persistentInfo.sequence << "timestamp=" << abtractTran.persistentInfo.timestamp;
#endif

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

}
