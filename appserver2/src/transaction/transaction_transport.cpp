#include "transaction_transport.h"
#include "transaction_message_bus.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "transaction_log.h"

namespace ec2
{

static const int SOCKET_TIMEOUT = 1000 * 1000;

QnTransactionTransport::QnTransactionTransport(bool isOriginator, bool isClient, QSharedPointer<AbstractStreamSocket> socket, const QUuid& removeGuid):
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
    m_removeGuid(removeGuid)
{
}


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
    if (state == ReadyForStreaming) {
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
    {
        QMutexLocker lock(&m_mutex);
        closeSocket();
        m_readSync = false;
        m_writeSync = false;
        m_dataToSend.clear();
    }
    if (m_originator) 
        setState(State::Connect);
    else
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
                        if (m_readBufferLen == fullChunkLen) {
                            QByteArray dataCopy;
                            dataCopy.append((const char *) rBuffer + m_chunkHeaderLen, m_chunkLen);
                            emit gotTransaction(dataCopy);
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
                        setState(State::Error);
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

    m_httpClient->doGet(remoteAddr);
}

void QnTransactionTransport::processTransactionData( const QByteArray& data)
{
    setState(ReadyForStreaming);
    m_chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    m_chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &m_chunkLen);
    while (m_chunkHeaderLen >= 0) 
    {
        int fullChunkLen = m_chunkHeaderLen + m_chunkLen + 2;
        if (bufferLen >= fullChunkLen)
        {
            QByteArray dataCopy;
            dataCopy.append(QByteArray::fromRawData((const char *) buffer + m_chunkHeaderLen, m_chunkLen));
            emit gotTransaction(dataCopy);
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

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    nx_http::AsyncHttpClient::State state = client->state();
    if (state == nx_http::AsyncHttpClient::sFailed)
        setState(State::Error);
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    nx_http::HttpHeaders::const_iterator itr = client->response()->headers.find("guid");
    if (itr != client->response()->headers.end()) {
        QMutexLocker lock(&m_mutex);
        m_removeGuid = itr->second;
    }
    else {
        setState(State::Error);
        return;
    }

    QByteArray data = m_httpClient->fetchMessageBodyBuffer();
    if (!data.isEmpty())
        processTransactionData(data);
    m_socket = m_httpClient->takeSocket();
    m_httpClient.reset();

    setState(QnTransactionTransport::Connected);
}

bool QnTransactionTransport::isReadSync() const
{
    QMutexLocker lock(&m_mutex);
    return m_readSync;
}

void QnTransactionTransport::setReadSync(bool value)
{
    QMutexLocker lock(&m_mutex);
    m_readSync = value;
}

bool QnTransactionTransport::isWriteSync() const
{
    QMutexLocker lock(&m_mutex);
    return m_writeSync;
}

void QnTransactionTransport::setWriteSync(bool value)
{
    QMutexLocker lock(&m_mutex);
    m_writeSync = value;
}


}
