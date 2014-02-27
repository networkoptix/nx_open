#include "transaction_message_bus.h"
#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "ec2_connection.h"
#include "common/common_module.h"

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

void QnTransactionTransport::sendSyncRequest()
{
    // send sync request
    readSync = false;
    QnTransaction<QnTranState> requestTran(ApiCommand::tranSyncRequest, false);
    requestTran.params = transactionLog->getTransactionsState();
    
    QByteArray syncRequest;
    owner->serializeTransaction(syncRequest, requestTran);
    addData(syncRequest);
}

void QnTransactionTransport::processError()
{
    owner->lock();

    readSync = false;
    writeSync = false;
    closeSocket();
    if (isConnectionOriginator) 
        state = State::Connect;
    else
        state = State::Closed;
    dataToSend.clear();
    
    QSharedPointer<QnTransactionTransport> sibling = owner->getSibling(this);
    if (sibling && sibling->state == ReadyForStreaming) 
        sibling->processError();

    owner->unlock();
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
            if (state == ReadyForStreaming) {
                int toRead = chunkHeaderLen == 0 ? 20 : (chunkHeaderLen + chunkLen + 2 - readBufferLen);
                ensureSize(readBuffer, toRead + readBufferLen);
                quint8* rBuffer = &readBuffer[0];
                readed = socket->recv(rBuffer + readBufferLen, toRead);
                if (readed < 1) {
                    // no more data or error
                    if(readed == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock)
                        processError();
                    return; // no more data
                }
                readBufferLen += readed;
                if (chunkHeaderLen == 0)
                    chunkHeaderLen = getChunkHeaderEnd(rBuffer, readBufferLen, &chunkLen);
                if (chunkHeaderLen) {
                    const int fullChunkLen = chunkHeaderLen + chunkLen + 2;
                    if (readBufferLen == fullChunkLen) {
                        owner->gotTransaction(this, QByteArray::fromRawData((const char *) rBuffer + chunkHeaderLen, chunkLen));
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

    QUrl url(remoteAddr);
    QUrlQuery q = QUrlQuery(remoteAddr.query());
    if( isClientPeer ) {
        q.removeQueryItem("isClient");
        q.addQueryItem("isClient", QString());
        readSync = true;
    }
    remoteAddr.setQuery(q);

    httpClient->doGet(remoteAddr);
}

void QnTransactionTransport::processTransactionData( const QByteArray& data)
{
    state = ReadyForStreaming;
    chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &chunkLen);
    while (chunkHeaderLen >= 0) 
    {
        int fullChunkLen = chunkHeaderLen + chunkLen + 2;
        if (bufferLen >= fullChunkLen)
        {
            owner->gotTransaction(this, QByteArray::fromRawData((const char *) buffer + chunkHeaderLen, chunkLen));
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
    state = ReadyForStreaming;
    aio::AIOService::instance()->watchSocket( socket, PollSet::etRead, this );
}

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        processError();
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    nx_http::HttpHeaders::const_iterator itr = client->response()->headers.find("guid");
    if (itr != client->response()->headers.end()) {
        remoteGuid = itr->second;
        owner->moveOutgoingConnToMainList(this);
    }
    else {
        processError();
        return;
    }

    QByteArray data = httpClient->fetchMessageBodyBuffer();
    if (!data.isEmpty())
        processTransactionData(data);
    socket = httpClient->takeSocket();
    httpClient.reset();

    owner->lock();
    startStreaming();
    if (!isClientPeer)
        owner->sendSyncRequestIfRequired(this);
    owner->unlock();
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

QnTransactionMessageBus::QnTransactionMessageBus(): m_timer(0), m_thread(0), m_mutex(QMutex::Recursive)
{
    m_thread = new QThread();
    m_thread->setObjectName("QnTransactionMessageBusThread");
    m_thread->start();
    moveToThread(m_thread);

    m_timer = new QTimer();
    connect(m_timer, &QTimer::timeout, this, &QnTransactionMessageBus::at_timer);
    m_timer->start(1);
    connect(this, &QnTransactionMessageBus::sendGotTransaction, this, &QnTransactionMessageBus::at_gotTransaction,  Qt::QueuedConnection);
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

QSharedPointer<QnTransactionTransport> QnTransactionMessageBus::getSibling(QnTransactionTransport* transport)
{
    QnConnectionMap::iterator itr = m_connections.find(transport->remoteGuid);
    if (itr == m_connections.end())
        return QSharedPointer<QnTransactionTransport>();
    ConnectionsToPeer& data = itr.value();
    return transport->isConnectionOriginator ? data.incomeConn : data.outcomeConn;
}

void QnTransactionMessageBus::sendSyncRequestIfRequired(QnTransactionTransport* transport)
{
    QSharedPointer<QnTransactionTransport> subling = getSibling(transport);
    // if sync already done or in progress do not send new request
    if (!subling || !subling->readSync)
        transport->sendSyncRequest();
}

void QnTransactionMessageBus::at_gotTransaction(QnTransactionTransport* sender, QByteArray serializedTran)
{
    QnAbstractTransaction tran;
    InputBinaryStream<QByteArray> stream(serializedTran);
    if (!tran.deserialize(&stream)) {
        qWarning() << Q_FUNC_INFO << "Ignore bad transaction data. size=" << serializedTran.size();
        sender->processError();
        return;
    }

    if (tran.command == ApiCommand::tranSyncRequest) {
        onGotTransactionSyncRequest(sender, stream);
        return;
    }
    else if (tran.command == ApiCommand::tranSyncResponse) {
        onGotTransactionSyncResponse(sender);
        return;
    }
    
    if (!sender->readSync)
        return;

    if (m_handler && !m_handler->processByteArray(sender, serializedTran)) {
        sender->processError();
        return;
    }

    QMutexLocker lock(&m_mutex);
    // proxy incoming transaction to other peers.
    QByteArray chunkData;
    QnTransactionMessageBus::serializeTransaction(chunkData, serializedTran);

    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == sender->remoteGuid)
            continue;
        ConnectionsToPeer& connection = itr.value();
        connection.proxyIncomingTransaction(tran, chunkData);
    }
}

void QnTransactionMessageBus::sendTransactionInternal(const QnId& originGuid, const QByteArray& chunkData)
{
    QMutexLocker lock(&m_mutex);
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == originGuid)
            continue; // do not send transaction back to originator.
        ConnectionsToPeer& data = itr.value();
        data.sendOutgoingTran(chunkData);
    }
}

// ------------------ QnTransactionMessageBus::ConnectionsToPeer  -------------------

void QnTransactionMessageBus::ConnectionsToPeer::proxyIncomingTransaction(const QnAbstractTransaction& tran, const QByteArray& chunkData)
{
    // proxy data to connected clients only. You can update this function to peer-to-peer sync via proxy in future
    if (incomeConn && incomeConn->state == QnTransactionTransport::ReadyForStreaming && incomeConn->isClientPeer) {
        incomeConn->addData(chunkData);
    }
}

void QnTransactionMessageBus::ConnectionsToPeer::sendOutgoingTran(const QByteArray& chunkData)
{
    if (incomeConn && incomeConn->writeSync)
        incomeConn->addData(chunkData);
    else if (outcomeConn && outcomeConn->writeSync)
        outcomeConn->addData(chunkData);
}

// ------------------ QnTransactionMessageBus::CustomHandler -------------------

template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(const QnAbstractTransaction&  abstractTran, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<T2> tran(abstractTran);
    if (!QnBinary::deserialize(tran.params, &stream))
        return false;
    
    // trigger notification, update local data e.t.c
    if (!m_handler->processIncomingTransaction<T2>(tran, stream.buffer()))
        return false;

    return true;
}

void QnTransactionMessageBus::onGotTransactionSyncResponse(QnTransactionTransport* sender)
{
    sender->readSync = true;
}

bool QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream)
{
    sender->writeSync = true;

    QnTranState params;
    if (QnBinary::deserialize(params, &stream))
    {
        QList<QByteArray> transactions;
        const ErrorCode errorCode = transactionLog->getTransactionsAfter(params, transactions);
        if (errorCode == ErrorCode::ok) 
        {
            QnTransaction<int> tran(ApiCommand::tranSyncResponse, false);
            tran.params = 0;
            QByteArray chunkData;
            serializeTransaction(chunkData, tran);
            sender->addData(chunkData);

            foreach(const QByteArray& data, transactions) {
                QByteArray chunkData;
                serializeTransaction(chunkData, data);
                sender->addData(chunkData);
            }
            return true;
        }
        else {
            qWarning() << "Can't execute query for sync with server peer!";
        }
    }
    return false;
}

template <class T>
bool QnTransactionMessageBus::CustomHandler<T>::processByteArray(QnTransactionTransport* sender, const QByteArray& data)
{
    Q_ASSERT(data.size() > 4);
    InputBinaryStream<QByteArray> stream(data);
    QnAbstractTransaction  abstractTran;
    if (!QnBinary::deserialize(abstractTran, &stream))
        return false; // bad data
    if (abstractTran.persistent && transactionLog && transactionLog->contains(abstractTran.id))
        return true; // transaction already processed

    switch (abstractTran.command)
    {
        case ApiCommand::getAllDataList:
            return deliveryTransaction<ApiFullData>(abstractTran, stream);

        //!ApiSetResourceStatusData
        case ApiCommand::setResourceStatus:
            return deliveryTransaction<ApiSetResourceStatusData>(abstractTran, stream);
        //!ApiSetResourceDisabledData
        case ApiCommand::setResourceDisabled:
            return deliveryTransaction<ApiSetResourceDisabledData>(abstractTran, stream);
        //!ApiResourceParams
        case ApiCommand::setResourceParams:
            return deliveryTransaction<ApiResourceParams>(abstractTran, stream);
        case ApiCommand::saveResource:
            return deliveryTransaction<ApiResourceData>(abstractTran, stream);
        case ApiCommand::removeResource:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
        case ApiCommand::setPanicMode:
            return deliveryTransaction<ApiPanicModeData>(abstractTran, stream);
            
        case ApiCommand::saveCamera:
            return deliveryTransaction<ApiCameraData>(abstractTran, stream);
        case ApiCommand::saveCameras:
            return deliveryTransaction<ApiCameraDataList>(abstractTran, stream);
        case ApiCommand::removeCamera:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
        case ApiCommand::addCameraHistoryItem:
            return deliveryTransaction<ApiCameraServerItemData>(abstractTran, stream);

        case ApiCommand::saveMediaServer:
            return deliveryTransaction<ApiMediaServerData>(abstractTran, stream);
        case ApiCommand::removeMediaServer:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveUser:
            return deliveryTransaction<ApiUserData>(abstractTran, stream);
        case ApiCommand::removeUser:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveBusinessRule:
            return deliveryTransaction<ApiBusinessRuleData>(abstractTran, stream);
        case ApiCommand::removeBusinessRule:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);

        case ApiCommand::saveLayouts:
            return deliveryTransaction<ApiLayoutDataList>(abstractTran, stream);
        case ApiCommand::saveLayout:
            return deliveryTransaction<ApiLayoutData>(abstractTran, stream);
        case ApiCommand::removeLayout:
            return deliveryTransaction<ApiIdData>(abstractTran, stream);
            
        case ApiCommand::addStoredFile:
        case ApiCommand::updateStoredFile:
            return deliveryTransaction<ApiStoredFileData>(abstractTran, stream);
        case ApiCommand::removeStoredFile:
            return deliveryTransaction<ApiStoredFilePath>(abstractTran, stream);

        case ApiCommand::broadcastBusinessAction:
            return deliveryTransaction<ApiBusinessActionData>(abstractTran, stream);

        default:
            break;
    }

    //TODO/IMPL deliver businessActionBroadcasted( const QnAbstractBusinessActionPtr& businessAction );
    //TODO/IMPL deliver businessRuleReset( const QnBusinessEventRuleList& rules );
    //TODO/IMPL deliver runtimeInfoChanged( const ec2::QnRuntimeInfo& runtimeInfo );

    return true;
}

void QnTransactionMessageBus::processConnState(QSharedPointer<QnTransactionTransport> &transport)
{
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
        transport.clear();
        break;
    }
}

void QnTransactionMessageBus::moveOutgoingConnToMainList(QnTransactionTransport* transport)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < m_connectingConnections.size(); ++i)
    {
        if (m_connectingConnections[i].data() == transport) {
            m_connections[transport->remoteGuid].outcomeConn = m_connectingConnections[i];
            m_connectingConnections.removeAt(i);
            break;
        }
    }
}

void QnTransactionMessageBus::at_timer()
{
    QMutexLocker lock(&m_mutex);
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end();)
    {
        ConnectionsToPeer& c = itr.value();
        if (c.incomeConn)
            processConnState(itr.value().incomeConn);
        if (c.outcomeConn)
            processConnState(itr.value().outcomeConn);
            
        if (!c.incomeConn && !c.outcomeConn)
            itr = m_connections.erase(itr);
        else {
            ++itr;
        }
    }
    for (int i = m_connectingConnections.size() -1; i >= 0; --i) {
        processConnState(m_connectingConnections[i]);
        if (!m_connectingConnections[i])
            m_connectingConnections.removeAt(i);
    }
}

void QnTransactionMessageBus::toFormattedHex(quint8* dst, quint32 payloadSize)
{
    for (;payloadSize; payloadSize >>= 4) {
        quint8 digit = payloadSize % 16;
        *dst-- = digit < 10 ? digit + '0': digit + 'A'-10;
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket, const QUrlQuery& query)
{
    bool isClient = query.hasQueryItem("isClient");
    QUuid remoteGuid  = query.queryItemValue(lit("guid"));

    if (remoteGuid.isNull()) {
        qWarning() << "Invalid incoming request. GUID must be filled!";
        return;
    }

    QnTransaction<ApiFullData> data;
    if (isClient) 
    {
        data.command = ApiCommand::getAllDataList;
        const ErrorCode errorCode = dbManager->doQuery(nullptr, data.params);
        if (errorCode != ErrorCode::ok) {
            qWarning() << "Can't execute query for sync with client peer!";
            return;
        }
    }


    QnTransactionTransport* transport = new QnTransactionTransport(this);
    transport->remoteGuid = remoteGuid;
    transport->isConnectionOriginator = false;
    transport->socket = socket;
    int handle = socket->handle();

    transport->startStreaming();

    // send fullsync to a client immediatly
    if (isClient) 
    {
        transport->isClientPeer = true;
        transport->writeSync = true;
        QByteArray buffer;
        serializeTransaction(buffer, data);
        transport->addData(buffer);
    }
    
    QMutexLocker lock(&m_mutex);
    m_connections[remoteGuid].incomeConn.reset(transport);
    if (!isClient)
        sendSyncRequestIfRequired(transport);
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url, bool isClient)
{
    QMutexLocker lock(&m_mutex);
    
    QnTransactionTransport* transport = new QnTransactionTransport(this);
    transport->isConnectionOriginator = true;
    transport->isClientPeer = isClient;
    transport->remoteAddr = url;
    transport->state = QnTransactionTransport::Connect;
    m_connectingConnections <<  QSharedPointer<QnTransactionTransport>(transport);
}

void QnTransactionMessageBus::removeConnectionFromPeer(const QUrl& url)
{
    QMutexLocker lock(&m_mutex);
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        ConnectionsToPeer& data = itr.value();
        if (data.outcomeConn && data.outcomeConn->remoteAddr == url) {
            data.outcomeConn->state = QnTransactionTransport::Closed;
            break;
        }
    }
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;
template class QnTransactionMessageBus::CustomHandler<Ec2DirectConnection>;

}
