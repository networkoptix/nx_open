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

void QnTransactionTransport::processError()
{
    closeSocket();
    if (isConnectionOriginator) 
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
            if (state == ReadyForStreaming) {
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
    else {
        nx_http::HttpHeaders::const_iterator itr = client->response()->headers.find("guid");
        if (itr != client->response()->headers.end()) {
            remoteGuid = itr->second;
            owner->moveOutgoingConnToMainList(this);
        }
        else {
            processError();
        }
    }
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    QByteArray data = httpClient->fetchMessageBodyBuffer();
    if (!data.isEmpty())
        processTransactionData(data);
    socket = httpClient->takeSocket();
    httpClient.reset();

    owner->sendSyncRequestIfRequired(this);
}

// --------------------------------- QnTransactionMessageBus ------------------------------

void QnTransactionMessageBus::sendSyncRequestIfRequired(QnTransactionTransport* transport)
{
    QMutexLocker lock(&m_mutex);
    transport->startStreaming();

    QnConnectionMap::iterator itr = m_connections.find(transport->remoteGuid);
    if (itr != m_connections.end())
    {
        ConnectionsToPeer& data = itr.value();
        QSharedPointer<QnTransactionTransport> subling = transport->isConnectionOriginator ? data.incomeConn : data.outcomeConn;
        if (subling && (subling->state == QnTransactionTransport::ReadyForStreaming || subling->state == QnTransactionTransport::WaitForTranSync))
            return; // sync already done or in progress

        // send sync request
        QnTransaction<QnTranState> requestTran;
        requestTran.initNew(ApiCommand::tranSyncRequest, false);
        requestTran.params = transactionLog->getTransactionsState();
        QByteArray syncRequest;
        OutputBinaryStream<QByteArray> stream(&syncRequest);
        QnBinary::serialize(requestTran, &stream);
        transport->addData(syncRequest);
        transport->state = QnTransactionTransport::WaitForTranSync;
    }
}

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

void QnTransactionMessageBus::ConnectionsToPeer::proxyIncomingTransaction(const QnAbstractTransaction& tran, const QByteArray& data)
{
    // proxy data to connected clients only. You can update this function to peer-to-peer sync via proxy in future
    if (incomeConn && incomeConn->state == QnTransactionTransport::ReadyForStreaming && incomeConn->isClientPeer)
        incomeConn->addData(data);
}

void QnTransactionMessageBus::ConnectionsToPeer::sendOutgoingTran(const QByteArray& data)
{
    if (incomeConn && incomeConn->state   == QnTransactionTransport::WaitForTranSync ||
        outcomeConn && outcomeConn->state == QnTransactionTransport::WaitForTranSync)
        return; // tcp is connected, but initialSync isn't done yet. do not send data for a while

    if (incomeConn && incomeConn->state == QnTransactionTransport::ReadyForStreaming)
        incomeConn->addData(data);
    else if (outcomeConn && outcomeConn->state == QnTransactionTransport::ReadyForStreaming)
        outcomeConn->addData(data);
}

void QnTransactionMessageBus::at_gotTransaction(QnTransactionTransport* sender, QByteArray data)
{
    QnAbstractTransaction tran;
    InputBinaryStream<QByteArray> stream(data);
    if (!tran.deserialize(&stream)) {
        qWarning() << Q_FUNC_INFO << "Ignore bad transaction data. size=" << data.size();
        return;
    }

    QMutexLocker lock(&m_mutex);

    // proxy incoming transaction to other peers.
    for(QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == sender->remoteGuid)
            continue;
        ConnectionsToPeer& connection = itr.value();
        connection.proxyIncomingTransaction(tran, data);
    }

    if (m_handler)
        m_handler->processByteArray(sender, data);
}

void QnTransactionMessageBus::sendTransactionInternal(const QnId& originGuid, const QByteArray& buffer)
{
    QMutexLocker lock(&m_mutex);
    for (QnConnectionMap::iterator itr = m_connections.begin(); itr != m_connections.end(); ++itr)
    {
        if (itr.key() == originGuid)
            continue; // do not send transaction back to originator.
        ConnectionsToPeer& data = itr.value();
        data.sendOutgoingTran(buffer);
    }
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

bool QnTransactionMessageBus::onGotTransactionSyncRequest(QnTransactionTransport* sender, InputBinaryStream<QByteArray>& stream)
{
    sender->state = QnTransactionTransport::ReadyForStreaming;

    QnTranState params;
    if (QnBinary::deserialize(params, &stream))
    {
        QList<QByteArray> transactions;
        const ErrorCode errorCode = transactionLog->getTransactionsAfter(params, transactions);
        if (errorCode == ErrorCode::ok) 
        {
            foreach(const QByteArray& data, transactions)
                sender->addData(data);
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
    if (abstractTran.persistent && transactionLog->contains(abstractTran.id))
        return true; // transaction already processed

    switch (abstractTran.command)
    {
        case ApiCommand::tranSyncRequest:
            return QnTransactionMessageBus::onGotTransactionSyncRequest(sender, stream);

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
    /*
    QByteArray tranStateBin = QByteArray::fromBase64(query.queryItemValue("tran_state").toLocal8Bit());
    QnTranState state;
    if (!tranStateBin.isEmpty())
        state = transactionLog->deserializeState(tranStateBin);
    */

    QnTransactionTransport* transport = new QnTransactionTransport(this);
    transport->remoteGuid = remoteGuid;
    transport->isConnectionOriginator = false;
    transport->socket = socket;
    int handle = socket->handle();

    QMutexLocker lock(&m_mutex);

    transport->startStreaming();

    /*
    // if we got connection from remote server and exists other connection where we try connect to the same peer, cancel outgoing connection
    for (int i  = 0; i < m_connections.size(); ++i)
    {
        //if (m_connections[i]->remoteGuid == remoteGuid && m_connections[i]->isConnectionOriginator &&  m_connections[i]->remoteGuid > qnCommon->moduleGUID()) 
        if (m_connections[i]->remoteGuid == remoteGuid && m_connections[i]->isConnectionOriginator &&  m_connections[i]->state < QnTransactionTransport::ReadyForStreaming)
        {
            delete m_connections[i];
            m_connections.removeAt(i);
            break;
        }
    }
    */

    
    // send fullsync to a client immediatly
    if (isClient) 
    {
        transport->isClientPeer = true;
        QnTransaction<ApiFullData> data;
        data.command = ApiCommand::getAllDataList;
        const ErrorCode errorCode = dbManager->doQuery(nullptr, data.params);
        if (errorCode == ErrorCode::ok) {
            QByteArray buffer;
            serializeTransaction(buffer, data);
            transport->addData(buffer);
        }
        else {
            qWarning() << "Can't execute query for sync with client peer!";
        }
    }
    else {
        sendSyncRequestIfRequired(transport);
    }
    m_connections[remoteGuid].incomeConn.reset(transport);
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url, bool isClient)
{
    QMutexLocker lock(&m_mutex);
    
    QString test = url.toString();

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
