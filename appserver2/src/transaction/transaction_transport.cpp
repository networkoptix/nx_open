#include "transaction_transport.h"
#include "transaction_message_bus.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "transaction_log.h"
#include "transaction_transport_serializer.h"
#include "common/common_module.h"

namespace ec2
{

static const int SOCKET_TIMEOUT = 1000 * 1000;

QSet<QUuid> QnTransactionTransport::m_existConn;
QnTransactionTransport::ConnectingInfoMap QnTransactionTransport::m_connectingConn;
QMutex QnTransactionTransport::m_staticMutex;

QnTransactionTransport::QnTransactionTransport(bool isOriginator, bool isClient, QSharedPointer<AbstractStreamSocket> socket, const QUuid& remoteGuid):
    m_socket(socket),
    m_state(NotDefined), 
    m_lastConnectTime(0), 
    m_readBufferLen(0), 
    m_chunkHeaderLen(0), 
    m_sendOffset(0), 
    m_chunkLen(0), 
    m_originator(isOriginator), 
    m_isClientPeer(isClient),
    m_readSync(false), 
    m_writeSync(false),
    m_remoteGuid(remoteGuid),
    m_timeDiff(0),
    m_connected(false)
{
}


QnTransactionTransport::~QnTransactionTransport()
{
    if( m_httpClient )
        m_httpClient->terminate();

    closeSocket();
}

void QnTransactionTransport::ensureSize(std::vector<quint8>& buffer, int size)
{
    if (buffer.size() < size)
        buffer.resize(size);
}

void QnTransactionTransport::addData(const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    if (m_dataToSend.isEmpty() && m_socket)
        aio::AIOService::instance()->watchSocket( m_socket, PollSet::etWrite, this );
    m_dataToSend.push_back(data);
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
    if (m_socket) {
        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etRead, this );
        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite, this );
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
            connectDone(m_remoteGuid);
        m_connected = false;
    }
    else if (state == ReadyForStreaming) {
        m_socket->setRecvTimeout(SOCKET_TIMEOUT);
        m_socket->setSendTimeout(SOCKET_TIMEOUT);
        m_socket->setNonBlockingMode(true);
        m_chunkHeaderLen = 0;
        aio::AIOService::instance()->watchSocket( m_socket, PollSet::etRead, this );
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
                if (m_state == ReadyForStreaming) {
                    int toRead = m_chunkHeaderLen == 0 ? 20 : (m_chunkHeaderLen + m_chunkLen + 2 - m_readBufferLen);
                    ensureSize(m_readBuffer, toRead + m_readBufferLen);
                    quint8* rBuffer = &m_readBuffer[0];
                    readed = m_socket->recv(rBuffer + m_readBufferLen, toRead);
                    if (readed < 1) {
                        // no more data or error
                        if(readed == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock)
                            setState(State::Error);
                        return; // no more data
                    }
                    m_readBufferLen += readed;
                    if (m_chunkHeaderLen == 0)
                        m_chunkHeaderLen = getChunkHeaderEnd(rBuffer, m_readBufferLen, &m_chunkLen);
                    if (m_chunkHeaderLen) {
                        const int fullChunkLen = m_chunkHeaderLen + m_chunkLen + 2;
                        if (m_readBufferLen == fullChunkLen) 
                        {
                            QSet<QnId> processedPeers;
                            QByteArray serializedTran;
                            QnTransactionTransportSerializer::deserialize(rBuffer + m_chunkHeaderLen, m_chunkLen, processedPeers, serializedTran);
                            emit gotTransaction(serializedTran, processedPeers);
                            m_readBufferLen = m_chunkHeaderLen = 0;
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
            QMutexLocker lock(&m_mutex);
            while (!m_dataToSend.isEmpty())
            {
                QByteArray& data = m_dataToSend.front();
                const char* dataStart = data.data();
                const char* dataEnd = dataStart + data.size();
                int sended = m_socket->send(dataStart + m_sendOffset, data.size() - m_sendOffset);
                if (sended < 1) {
                    if(sended == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock) {
                        m_sendOffset = 0;
                        aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite, this );
                        setStateNoLock(State::Error);
                    }
                    return; // can't send any more
                }
                m_sendOffset += sended;
                if (m_sendOffset == data.size()) {
                    m_sendOffset = 0;
                    m_dataToSend.dequeue();
                }
            }
            aio::AIOService::instance()->removeFromWatch( m_socket, PollSet::etWrite, this );
            break;
        }
    case PollSet::etTimedOut:
    case PollSet::etError:
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
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnTransactionTransport::at_responseReceived, Qt::DirectConnection);
    connect(m_httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnTransactionTransport::at_httpClientDone, Qt::DirectConnection);

    QUrlQuery q = QUrlQuery(remoteAddr.query());
    if( m_isClientPeer ) {
        q.removeQueryItem("isClient");
        q.addQueryItem("isClient", QString());
        setReadSync(true);
    }
    remoteAddr.setQuery(q);
    m_remoteAddr = remoteAddr;
    m_httpClient->doGet(remoteAddr);
}

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        setState(State::Error);
}

/*
void QnTransactionTransport::getPeerInfo(const QnId& id, bool* isExist, bool* isConnecting)
{
    //QMutexLocker lock(&m_staticMutex);
    *isExist = m_existConn.contains(id);
    *isConnecting = m_connectingConn.contains(id);
}



void QnTransactionTransport::connectInProgress(const QnId& id)
{
    QMutexLocker lock(&m_staticMutex);
    m_connectingConn << id;
}
*/

bool QnTransactionTransport::tryAcquireConnecting(const QnId& remoteGuid, bool isOriginator)
{
    QMutexLocker lock(&m_staticMutex);

    bool isExist = m_existConn.contains(remoteGuid);
    //bool isConnecting = m_connectingConn.contains(remoteGuid);
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
        m_connectingConn.remove(remoteGuid);
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
    m_httpClient->doGet(m_remoteAddr);
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find("guid");
    nx_http::HttpHeaders::const_iterator itrTime = client->response()->headers.find("time");

    bool ok1 = itrGuid != client->response()->headers.end();
    bool ok2 = itrGuid != client->response()->headers.end();

    if (itrGuid == client->response()->headers.end() || itrTime == client->response()->headers.end() || client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        if (getState() == ConnectingStage2)
            QnTransactionTransport::connectingCanceled(m_remoteGuid, true);
        setState(State::Error);
        return;
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();

    if (getState() == ConnectingStage1) {
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(m_remoteGuid, true);
        if (lockOK) {
            m_remoteGuid = itrGuid->second;
            if (QnTransactionLog::instance()) {
                qint64 localTime = QnTransactionLog::instance()->getRelativeTime();
                qint64 remoteTime = itrTime->second.toLongLong();
                setTimeDiff(localTime - remoteTime);
            }
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
        m_httpClient.reset();
        if (QnTransactionTransport::tryAcquireConnected(m_remoteGuid, true)) {
            if (!data.isEmpty())
                processTransactionData(data);
            setState(QnTransactionTransport::Connected);
        }
        else {
            setState(QnTransactionTransport::Error);
        }
    }
}

void QnTransactionTransport::processTransactionData( const QByteArray& data)
{
    m_chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    m_chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &m_chunkLen);
    while (m_chunkHeaderLen >= 0) 
    {
        int fullChunkLen = m_chunkHeaderLen + m_chunkLen + 2;
        if (bufferLen >= fullChunkLen)
        {
            QSet<QnId> processedPeers;
            QByteArray serializedTran;
            QnTransactionTransportSerializer::deserialize(buffer + m_chunkHeaderLen, m_chunkLen, processedPeers, serializedTran);
            emit gotTransaction(serializedTran, processedPeers);

            buffer += fullChunkLen;
            bufferLen -= fullChunkLen;
        }
        else {
            break;
        }
        m_chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &m_chunkLen);
    }

    if (bufferLen > 0) {
        m_readBuffer.resize(bufferLen);
        memcpy(&m_readBuffer[0], buffer, bufferLen);
    }
    m_readBufferLen = bufferLen;
}

}
