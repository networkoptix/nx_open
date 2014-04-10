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
    m_lastConnectTime(0), 
    m_readSync(false), 
    m_writeSync(false),
    m_remoteGuid(remoteGuid),
    m_originator(isOriginator), 
    m_isClientPeer(isClient),
    m_socket(socket),
    m_state(NotDefined), 
    m_readBufferLen(0), 
    m_chunkLen(0), 
    m_sendOffset(0), 
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

void QnTransactionTransport::ensureSize(std::vector<quint8>& buffer, std::size_t size)
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
        m_chunkLen = 0;
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
                    int toRead = m_chunkLen == 0 ? 4 : (m_chunkLen - m_readBufferLen);
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
                    if (m_chunkLen == 0 && m_readBufferLen >= 4) {
                        quint32* chunkLenField = (quint32*) rBuffer;
                        m_chunkLen = ntohl(*chunkLenField);
                    }
                    if (m_chunkLen) {
                        if (m_readBufferLen == m_chunkLen) 
                        {
                            QSet<QnId> processedPeers;
                            QByteArray serializedTran;
                            QnTransactionTransportSerializer::deserializeTran(rBuffer + 4, m_chunkLen - 4, processedPeers, serializedTran);
                            emit gotTransaction(serializedTran, processedPeers);
                            m_readBufferLen = m_chunkLen = 0;
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

    m_httpClient->setUserName("system");
    m_httpClient->setUserPassword(qnCommon->getSystemPassword());
    if (!remoteAddr.userName().isEmpty())
    {
        remoteAddr.setUserName(QString());
        remoteAddr.setPassword(QString());
    }

    QUrlQuery q = QUrlQuery(remoteAddr.query());
    if( m_isClientPeer ) {
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
        QnTransactionTransport::connectingCanceled(m_remoteGuid, true);
    qWarning() << Q_FUNC_INFO << "Connection canceled from state " << getState();
    setState(Error);
}

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        cancelConnecting();
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    nx_http::HttpHeaders::const_iterator itrGuid = client->response()->headers.find("guid");
    nx_http::HttpHeaders::const_iterator itrTime = client->response()->headers.find("time");

    if (itrGuid == client->response()->headers.end() || itrTime == client->response()->headers.end() || client->response()->statusLine.statusCode != nx_http::StatusCode::ok)
    {
        cancelConnecting();
        return;
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();
    m_remoteGuid = itrGuid->second;

    if (getState() == ConnectingStage1) {
        bool lockOK = QnTransactionTransport::tryAcquireConnecting(m_remoteGuid, true);
        if (lockOK) {
            if (QnTransactionLog::instance()) {
                qint64 localTime = QnTransactionLog::instance()->getRelativeTime();
                qint64 remoteTime = itrTime->second.toLongLong();
                if (remoteTime != -1)
                    setTimeDiff(remoteTime - localTime);
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
            cancelConnecting();
        }
    }
}

void QnTransactionTransport::processTransactionData( const QByteArray& data)
{
    m_chunkLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    if (bufferLen >= 4) {
        quint32* chunkLenField = (quint32*) buffer;
        m_chunkLen = ntohl(*chunkLenField);
    }
    while (m_chunkLen >= 0) 
    {
        if (bufferLen >= m_chunkLen)
        {
            QSet<QnId> processedPeers;
            QByteArray serializedTran;
            QnTransactionTransportSerializer::deserializeTran(buffer + 4, m_chunkLen - 4, processedPeers, serializedTran);
            emit gotTransaction(serializedTran, processedPeers);

            buffer += m_chunkLen;
            bufferLen -= m_chunkLen;
            m_chunkLen = 0;
        }
        else {
            break;
        }
        if (bufferLen >= 4) {
            quint32* chunkLenField = (quint32*) buffer;
            m_chunkLen = ntohl(*chunkLenField);
        }
    }

    if (bufferLen > 0) {
        m_readBuffer.resize(bufferLen);
        memcpy(&m_readBuffer[0], buffer, bufferLen);
    }
    m_readBufferLen = bufferLen;
}

}
