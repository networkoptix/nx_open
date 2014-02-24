#include "transaction_message_bus.h"
#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"

namespace ec2
{

static const int SOCKET_TIMEOUT = 1000 * 1000;
static const int RECONNECT_TIMEOUT = 1000 * 5;

// --------------------------------- QnTransactionTransport ------------------------------

QnTransactionTransport::~QnTransactionTransport()
{
    closeSocket();
}

void QnTransactionTransport::ensureSize(std::vector<quint8>& buffer, int size)
{
    if (buffer.size() < size)
        buffer.resize(size);
}

void QnTransactionTransport::addData(const QByteArray& data)
{
    QMutexLocker lock(&mutex);
    if (dataToSend.isEmpty() && socket)
        aio::AIOService::instance()->watchSocket( socket, PollSet::etWrite, this );
    dataToSend.push_back(data);
}

int QnTransactionTransport::getChunkHeaderEnd(const quint8* data, int dataLen, quint32* const size)
{
    *size = 0;
    for (int i = 0; i < dataLen - 1; ++i)
    {
        if (data[i] >= '0' && data[i] <= '9')
            *size = *size * 16 + (data[i] - '0');
        else if (data[i] >= 'a' && data[i] <= 'f')
            *size = *size * 16 + (data[i] - 'a' + 10);
        else if (data[i] >= 'A' && data[i] <= 'F')
            *size = *size * 16 + (data[i] - 'A' + 10);
        else if (data[i] == '\r' && data[i+1] == '\n')
            return i + 2;
    }
    return -1;
}

void QnTransactionTransport::closeSocket()
{
    if (socket) {
        socket->close();
        aio::AIOService::instance()->removeFromWatch( socket, PollSet::etRead, this );
        aio::AIOService::instance()->removeFromWatch( socket, PollSet::etWrite, this );
        socket.reset();
    }
}

void QnTransactionTransport::processError()
{
    closeSocket();
    if (isClientSide) 
        state = State::Connect;
    else
        state = State::Closed;
} 

void QnTransactionTransport::eventTriggered( AbstractSocket* , PollSet::EventType eventType ) throw()
{
    //AbstractStreamSocket* streamSock = (AbstractStreamSocket*) sock;
    int readed;
    switch( eventType )
    {
    case PollSet::etRead:
    {
        while (1)
        {
            if (state == ReadChunks) {
                int toRead = chunkHeaderLen == 0 ? 20 : (chunkHeaderLen + chunkLen + 2 - readBufferLen);
                ensureSize(readBuffer, toRead + readBufferLen);
                quint8* rBuffer = &readBuffer[0];
                readed = socket->recv(rBuffer + readBufferLen, toRead);
                if (readed < 1) {
                    // no more data or error
                    if(readed == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock)
                        //SystemError::ErrorCode err = SystemError::getLastOSErrorCode();
                            //if(err != SystemError::wouldBlock && err != SystemError::noError)
                                processError();
                    return; // no more data
                }
                readBufferLen += readed;
                if (chunkHeaderLen == 0)
                    chunkHeaderLen = getChunkHeaderEnd(rBuffer, readBufferLen, &chunkLen);
                if (chunkHeaderLen) {
                    const int fullChunkLen = chunkHeaderLen + chunkLen + 2;
                    if (readBufferLen == fullChunkLen) {
                        owner->gotTransaction(QByteArray::fromRawData((const char *) rBuffer + chunkHeaderLen, chunkLen));
                        readBufferLen = chunkHeaderLen = 0;
                    }
                }
            }
            else {
                break;
            }
        }
        break;
    }
    case PollSet::etWrite: 
    {
        QMutexLocker lock(&mutex);
        while (!dataToSend.isEmpty())
        {
            QByteArray& data = dataToSend.front();
            const char* dataStart = data.data();
            const char* dataEnd = dataStart + data.size();
            int sended = socket->send(dataStart + sendOffset, data.size() - sendOffset);
            if (sended < 1) {
                if(sended == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock) {
                    sendOffset = 0;
                    aio::AIOService::instance()->removeFromWatch( socket, PollSet::etWrite, this );
                    processError();
                }
                return; // can't send any more
            }
            sendOffset += sended;
            if (sendOffset == data.size()) {
                sendOffset = 0;
                dataToSend.dequeue();
            }
        }
        aio::AIOService::instance()->removeFromWatch( socket, PollSet::etWrite, this );
        break;
    }
    case PollSet::etTimedOut:
    case PollSet::etError:
        processError();
        break;
    default:
        break;
    }
}

void QnTransactionTransport::doClientConnect()
{
    httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    connect(httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnTransactionTransport::at_responseReceived, Qt::DirectConnection);
    connect(httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnTransactionTransport::at_httpClientDone, Qt::DirectConnection);
    httpClient->doGet(remoteAddr);
}

void QnTransactionTransport::processTransactionData( const QByteArray& data)
{
    state = ReadChunks;
    chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &chunkLen);
    while (chunkHeaderLen >= 0) 
    {
        int fullChunkLen = chunkHeaderLen + chunkLen + 2;
        if (bufferLen >= fullChunkLen)
        {
            owner->gotTransaction(QByteArray::fromRawData((const char *) buffer + chunkHeaderLen, chunkLen));
            buffer += fullChunkLen;
            bufferLen -= fullChunkLen;
        }
        else {
            break;
        }
        chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &chunkLen);
    }

    if (bufferLen > 0) {
        readBuffer.resize(bufferLen);
        memcpy(&readBuffer[0], buffer, bufferLen);
    }
    readBufferLen = bufferLen;
}

void QnTransactionTransport::startStreaming()
{
    socket->setRecvTimeout(SOCKET_TIMEOUT);
    socket->setSendTimeout(SOCKET_TIMEOUT);
    socket->setNonBlockingMode(true);
    chunkHeaderLen = 0;
    state = QnTransactionTransport::ReadChunks;
    int handle = socket->handle();
    aio::AIOService::instance()->watchSocket( socket, PollSet::etRead, this );
}

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        processError();
    else {
        nx_http::HttpHeaders::const_iterator itr = client->response()->headers.find("guid");
        if (itr != client->response()->headers.end())
            remoteGuid = itr->second;
    }
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    processTransactionData(httpClient->fetchMessageBodyBuffer());
    socket = httpClient->takeSocket();
    httpClient.reset();
    startStreaming();
}

// --------------------------------- QnTransactionMessageBus ------------------------------

static QnTransactionMessageBus*m_globalInstance = 0;

QnTransactionMessageBus* QnTransactionMessageBus::instance()
{
    return m_globalInstance;
}
void QnTransactionMessageBus::initStaticInstance(QnTransactionMessageBus* instance)
{
    Q_ASSERT(m_globalInstance == 0 || instance == 0);
    delete m_globalInstance;
    m_globalInstance = instance;
}

QnTransactionMessageBus::QnTransactionMessageBus(): m_timer(0), m_thread(0)
{
    m_thread = new QThread();
    m_thread->setObjectName("QnTransactionMessageBusThread");
    m_thread->start();
    moveToThread(m_thread);

    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::at_timer);
    m_timer->start(1);
}

QnTransactionMessageBus::~QnTransactionMessageBus()
{
    if (m_thread) {
        m_thread->exit();
        m_thread->wait();
    }
    delete m_thread;
    delete m_timer;
}

template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(ApiCommand::Value command, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<T2> tran;
    if (!tran.deserialize(command, &stream))
        return false;
    
    // trigger notification
    m_handler->processTransaction<T2>(tran); 

    return true;
}

template <class T>
bool QnTransactionMessageBus::CustomHandler<T>::processByteArray(const QByteArray& data)
{
    Q_ASSERT(data.size() > 4);
    InputBinaryStream<QByteArray> stream(data);
    ApiCommand::Value command;
    if (!QnBinary::deserialize(command, &stream))
        return false; // bad data
    switch (command)
    {
        case ApiCommand::getAllDataList:
            return deliveryTransaction<ApiFullData>(command, stream);

        //!ApiSetResourceStatusData
        case ApiCommand::setResourceStatus:
            return deliveryTransaction<ApiSetResourceStatusData>(command, stream);
        //!ApiSetResourceDisabledData
        case ApiCommand::setResourceDisabled:
            return deliveryTransaction<ApiSetResourceDisabledData>(command, stream);
        //!ApiResourceParams
        case ApiCommand::setResourceParams:
            return deliveryTransaction<ApiResourceParams>(command, stream);
        case ApiCommand::saveResource:
            return deliveryTransaction<ApiResourceData>(command, stream);
        case ApiCommand::removeResource:
            return deliveryTransaction<ApiIdData>(command, stream);
        case ApiCommand::setPanicMode:
            return deliveryTransaction<ApiPanicModeData>(command, stream);
            
        case ApiCommand::saveCamera:
            return deliveryTransaction<ApiCameraData>(command, stream);
        case ApiCommand::saveCameras:
            return deliveryTransaction<ApiCameraDataList>(command, stream);
        case ApiCommand::removeCamera:
            return deliveryTransaction<ApiIdData>(command, stream);
        case ApiCommand::addCameraHistoryItem:
            return deliveryTransaction<ApiCameraServerItemData>(command, stream);

        case ApiCommand::saveMediaServer:
            return deliveryTransaction<ApiMediaServerData>(command, stream);
        case ApiCommand::removeMediaServer:
            return deliveryTransaction<ApiIdData>(command, stream);

        case ApiCommand::saveUser:
            return deliveryTransaction<ApiUserData>(command, stream);
        case ApiCommand::removeUser:
            return deliveryTransaction<ApiIdData>(command, stream);

        case ApiCommand::saveBusinessRule:
            return deliveryTransaction<ApiBusinessRuleData>(command, stream);
        case ApiCommand::removeBusinessRule:
            return deliveryTransaction<ApiIdData>(command, stream);

        case ApiCommand::saveLayouts:
            return deliveryTransaction<ApiLayoutDataList>(command, stream);
        case ApiCommand::saveLayout:
            return deliveryTransaction<ApiLayoutData>(command, stream);
        case ApiCommand::removeLayout:
            return deliveryTransaction<ApiIdData>(command, stream);
            
        case ApiCommand::addStoredFile:
        case ApiCommand::updateStoredFile:
            return deliveryTransaction<ApiStoredFileData>(command, stream);
        case ApiCommand::removeStoredFile:
            return deliveryTransaction<ApiStoredFilePath>(command, stream);

        case ApiCommand::broadcastBusinessAction:
            return deliveryTransaction<ApiBusinessActionData>(command, stream);

        default:
            break;
    }

    //TODO/IMPL deliver businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
    //TODO/IMPL deliver businessRuleReset( const QnBusinessEventRuleList& rules );
    //TODO/IMPL deliver runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );

    return true;
}

void QnTransactionMessageBus::at_timer()
{
    for (int i = m_connections.size()-1; i >= 0; --i)
    {
        QnTransactionTransport* transport = m_connections[i];
        switch (transport->state) 
        {
            case QnTransactionTransport::Connect:
            {
                qint64 ct = QDateTime::currentDateTime().toMSecsSinceEpoch();
                if (ct - transport->lastConnectTime >= RECONNECT_TIMEOUT) {
                    transport->lastConnectTime = ct;
                    transport->state = QnTransactionTransport::Connecting;
                    transport->doClientConnect();
                }
                break;
            }
            case QnTransactionTransport::Closed:
                delete m_connections[i];
                m_connections.removeAt(i);
                break;
            case QnTransactionTransport::ReadyForStreaming:
                transport->startStreaming();
                break;
        }
    }
}

void QnTransactionMessageBus::toFormattedHex(quint8* dst, quint32 payloadSize)
{
    for (;payloadSize; payloadSize >>= 4) {
        quint8 digit = payloadSize % 16;
        *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QUuid& remoteGuid, bool doFullSync)
{
    QnTransactionTransport* transport = new QnTransactionTransport(this);
    transport->remoteGuid = remoteGuid;
    transport->isClientSide = false;
    transport->socket = socket;
    int handle = socket->handle();
    transport->state = QnTransactionTransport::ReadyForStreaming;

    QMutexLocker lock(&m_mutex);
    if (doFullSync) {
        QnTransaction<ApiFullData> data;
        data.command = ApiCommand::getAllDataList;
        const ErrorCode errorCode = dbManager->doQuery(nullptr, data.params);
        if (errorCode == ErrorCode::ok) {
            QByteArray buffer;
            serializeTransaction(buffer, data);
            transport->addData(buffer);
        }
    }
    m_connections.push_back(transport);
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    QString test = url.toString();

    QnTransactionTransport* transport = new QnTransactionTransport(this);
    transport->isClientSide = true;
    transport->remoteAddr = url;
    transport->state = QnTransactionTransport::Connect;
    m_connections.push_back(transport);
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    foreach(QnTransactionTransport* transport, m_connections)
    {
        if (transport->remoteAddr == url)
            transport->state = QnTransactionTransport::Closed;
    }
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;

}
