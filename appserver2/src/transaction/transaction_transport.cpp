#include "transaction_transport.h"

#include <QtCore/QUrlQuery>
#include <QtCore/QTimer>

#include "transaction_message_bus.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "transaction_log.h"
#include <transaction/chunked_transfer_encoder.h>
#include "common/common_module.h"


namespace ec2
{

static const int SOCKET_TIMEOUT = 1000 * 1000;

QSet<QUuid> QnTransactionTransport::m_existConn;
QnTransactionTransport::ConnectingInfoMap QnTransactionTransport::m_connectingConn;
QMutex QnTransactionTransport::m_staticMutex;

QnTransactionTransport::QnTransactionTransport(const ApiPeerData &localPeer, const ApiPeerData &remotePeer, const QSharedPointer<AbstractStreamSocket>& socket):
    m_localPeer(localPeer),
    m_remotePeer(remotePeer),
    m_lastConnectTime(0), 
    m_readSync(false), 
    m_writeSync(false),
    m_socket(socket),
    m_state(NotDefined), 
    m_readBufferLen(0),
    m_chunkHeaderLen(0),
    m_chunkLen(0), 
    m_sendOffset(0), 
    m_connected(false),
    m_prevGivenHandlerID(0)
{
    m_readBuffer.resize( 4*1024 );
}


QnTransactionTransport::~QnTransactionTransport()
{
    if( m_httpClient )
        m_httpClient->terminate();

    closeSocket();
}

void QnTransactionTransport::addData(QByteArray&& data)
{
    QMutexLocker lock(&m_mutex);
    if (m_dataToSend.empty() && m_socket)
        aio::AIOService::instance()->watchSocket( m_socket, aio::etWrite, this );
    m_dataToSend.push_back(std::move(data));
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
        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etRead, this );
        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite, this );
        m_socket->close();
        m_socket.reset();
    }
}

void QnTransactionTransport::setState(State state)
{
    QMutexLocker lock(&m_mutex);
    setStateNoLock(state);
}

void QnTransactionTransport::setStateNoLock(State state)
{
    if (state == Connected) {
        m_connected = true;
    }
    else if (state == Error) {
        if (m_connected)
            connectDone(m_remotePeer.id);
        m_connected = false;
    }
    else if (state == ReadyForStreaming) {
        m_socket->setRecvTimeout(SOCKET_TIMEOUT);
        m_socket->setSendTimeout(SOCKET_TIMEOUT);
        m_socket->setNonBlockingMode(true);
        m_chunkHeaderLen = 0;
        aio::AIOService::instance()->watchSocket( m_socket, aio::etRead, this );
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
        m_readSync = false;
        m_writeSync = false;
        m_dataToSend.clear();
    }
    setState(State::Closed);
}

void QnTransactionTransport::eventTriggered( AbstractSocket* , aio::EventType eventType ) throw()
{
    switch( eventType )
    {
    case aio::etRead:
        {
            while (1)
            {
                if (m_state == ReadyForStreaming) {
                    //TODO #ak it makes sense to use here some chunk parser class. At this moment http chunk parsing logic is implemented 
                        //3 times in different parts of our code (async http client, sync http server and here)
                        //Also, it is better to read all data from socket by as few system calls as possible (use some fixed size buffer and parse data from that buffer)
                    assert( m_readBufferLen < m_readBuffer.size() );
                    const int readed = m_socket->recv(&m_readBuffer[0] + m_readBufferLen, m_readBuffer.size() - m_readBufferLen);
                    if (readed < 1) {
                        // no more data or error
                        if(readed == 0 || (SystemError::getLastOSErrorCode() != SystemError::wouldBlock && SystemError::getLastOSErrorCode() != SystemError::again))
                            setState(State::Error);
                        return; // no more data
                    }
                    m_readBufferLen += readed;

                    for( size_t readBufPos = 0;; )
                    {
                        //processing available data: we can have zero or more chunks in m_readBuffer
                        nx_http::ChunkHeader httpChunkHeader;
                        if (m_chunkHeaderLen == 0)  //chunk header has not been read yet
                        {
                            //reading chunk header
                            m_chunkHeaderLen = readChunkHeader(&m_readBuffer[0] + readBufPos, m_readBufferLen, &httpChunkHeader);
                            if( m_chunkHeaderLen == 0 )
                            {
                                if( readBufPos > 0 )
                                    memmove( &m_readBuffer[0], &m_readBuffer[0] + readBufPos, m_readBufferLen );
                                break;  //not enough data in rBuffer to read http chunk header
                            }
                            m_chunkLen = httpChunkHeader.chunkSize;
                        }

                        assert( m_chunkHeaderLen > 0 );

                        //have just read http chunk
                        const size_t fullChunkSize = m_chunkHeaderLen + m_chunkLen + sizeof("\r\n")-1;
                        if( m_readBuffer.size() < fullChunkSize )
                            m_readBuffer.resize( fullChunkSize );
                        processChunkExtensions( httpChunkHeader );  //processing chunk extensions even before receiving whole transaction

                        if( m_readBufferLen < fullChunkSize )
                        {
                            //not enough data in rBuffer
                            if( readBufPos > 0 )
                                memmove( &m_readBuffer[0], &m_readBuffer[0] + readBufPos, m_readBufferLen );
                            break;
                        }

                        QByteArray serializedTran;
                        QnTransactionTransportHeader transportHeader;
                        QnBinaryTransactionSerializer::deserializeTran(
                            &m_readBuffer[0] + readBufPos + m_chunkHeaderLen + 4,
                            m_chunkLen - 4,
                            transportHeader,
                            serializedTran );
                        assert( !transportHeader.processedPeers.empty() );
                        emit gotTransaction(serializedTran, transportHeader);
                        readBufPos += fullChunkSize;
                        m_readBufferLen -= fullChunkSize;
                        m_chunkHeaderLen = 0;

                        if( !m_readBufferLen )
                            break;  //processed all data
                    }
                }
                else {
                    break;
                }
            }
            break;
        }
    case aio::etWrite:
        {
            QMutexLocker lock(&m_mutex);
            while (!m_dataToSend.empty())
            {
                DataToSend& dataCtx = m_dataToSend.front();
                if( dataCtx.encodedSourceData.isEmpty() )
                {
                    std::vector<nx_http::ChunkExtension> chunkExtensions;
                    addHttpChunkExtensions( &chunkExtensions );
                    dataCtx.encodedSourceData = QnChunkedTransferEncoder::serializedTransaction(
                        dataCtx.sourceData,
                        chunkExtensions );
                }

                const QByteArray& data = dataCtx.encodedSourceData;
                int sended = m_socket->send(data.constData() + m_sendOffset, data.size() - m_sendOffset);
                if (sended < 1) {
                    if(sended == 0 || (SystemError::getLastOSErrorCode() != SystemError::wouldBlock && SystemError::getLastOSErrorCode() != SystemError::again)) {
                        m_sendOffset = 0;
                        aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite, this );
                        setStateNoLock(State::Error);
                    }
                    return; // can't send any more
                }
                m_sendOffset += sended;
                if (m_sendOffset == data.size()) {
                    m_sendOffset = 0;
                    m_dataToSend.pop_front();
                }
            }
            aio::AIOService::instance()->removeFromWatch( m_socket, aio::etWrite, this );
            break;
        }
    case aio::etTimedOut:
    case aio::etError:
        setState(State::Error);
        break;
    default:
        break;
    }
}

void QnTransactionTransport::doOutgoingConnect(QUrl remoteAddr)
{
    setState(ConnectingStage1);
    m_httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    m_httpClient->setDecodeChunkedMessageBody( false ); //chunked decoding is done in this class
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnTransactionTransport::at_responseReceived, Qt::DirectConnection);
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnTransactionTransport::at_httpClientDone, Qt::DirectConnection);

    m_httpClient->setUserName("system");    
    m_httpClient->setUserPassword(qnCommon->getSystemPassword());
    if (!remoteAddr.userName().isEmpty())
    {
        remoteAddr.setUserName(QString());
        remoteAddr.setPassword(QString());
    }

    QUrlQuery q = QUrlQuery(remoteAddr.query());

    // Client reconnects to the server
    if( m_localPeer.peerType == Qn::PT_DesktopClient ) {
        q.removeQueryItem("isClient");
        q.addQueryItem("isClient", QString());
        setState(ConnectingStage2); // one GET method for client peer is enough
        setReadSync(true);
    } else if (m_localPeer.peerType == Qn::PT_VideowallClient) {
        q.removeQueryItem("isClient");  //videowall client is still client
        q.addQueryItem("isClient", QString());

        q.removeQueryItem("videowallGuid");
        q.addQueryItem("videowallGuid", m_localPeer.params["videowallGuid"]);

        q.removeQueryItem("instanceGuid");
        q.addQueryItem("instanceGuid", m_localPeer.params["instanceGuid"]);

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

bool QnTransactionTransport::tryAcquireConnecting(const QnId& remoteGuid, bool isOriginator)
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


void QnTransactionTransport::connectingCanceled(const QnId& remoteGuid, bool isOriginator)
{
    QMutexLocker lock(&m_staticMutex);
    connectingCanceledNoLock(remoteGuid, isOriginator);
}

void QnTransactionTransport::connectingCanceledNoLock(const QnId& remoteGuid, bool isOriginator)
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

bool QnTransactionTransport::tryAcquireConnected(const QnId& remoteGuid, bool isOriginator)
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

void QnTransactionTransport::connectDone(const QnId& id)
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

void QnTransactionTransport::at_httpClientDone(const nx_http::AsyncHttpClientPtr& client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        cancelConnecting();
}

void QnTransactionTransport::at_responseReceived(const nx_http::AsyncHttpClientPtr& client)
{
    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find("guid");

    if (itrGuid == client->response()->headers.end() || client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        cancelConnecting();
        return;
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();
    m_remotePeer.id = itrGuid->second;

    if (getState() == ConnectingStage1) {
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(m_remotePeer.id, true);
        if (lockOK) {
            setState(ConnectingStage2);
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
            if (!data.isEmpty())
                processTransactionData(data);
            setState(QnTransactionTransport::Connected);
        }
        else {
            cancelConnecting();
        }
    }
}

void QnTransactionTransport::processTransactionData(const QByteArray& data)
{
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
            QByteArray serializedTran;
            QnTransactionTransportHeader transportHeader;
            QnBinaryTransactionSerializer::deserializeTran(buffer + m_chunkHeaderLen + 4, m_chunkLen - 4, transportHeader, serializedTran);
            assert( !transportHeader.processedPeers.empty() );
            emit gotTransaction(serializedTran, transportHeader);

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
        if( m_readBuffer.size() < bufferLen )
            m_readBuffer.resize(bufferLen);
        memcpy(&m_readBuffer[0], buffer, bufferLen);
    }
    m_readBufferLen = bufferLen;
}

bool QnTransactionTransport::isReadyToSend(ApiCommand::Value command) const
{
     // allow to send system command immediately, without tranSyncRequest
    return ApiCommand::isSystem(command) ? true : m_writeSync;
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

}
