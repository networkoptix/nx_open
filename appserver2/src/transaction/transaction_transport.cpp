#include "transaction_transport.h"
#include "transaction_message_bus.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"
#include "transaction_log.h"

namespace ec2
{

static const int SOCKET_TIMEOUT = 1000 * 1000;

QnTransactionTransport::~QnTransactionTransport()
{
    Q_ASSERT(QThread::currentThread() == owner->thread());
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
        aio::AIOService::instance()->removeFromWatch( socket, PollSet::etRead, this );
        aio::AIOService::instance()->removeFromWatch( socket, PollSet::etWrite, this );
        socket->close();
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
    QnTransactionMessageBus::serializeTransaction(syncRequest, requestTran);
    addData(syncRequest);
}

void QnTransactionTransport::setState(State state)
{
    QMutexLocker lock(&mutex);
    this->state = state;
}

QnTransactionTransport::State QnTransactionTransport::getState() const
{
    QMutexLocker lock(&mutex);
    return state;
}

void QnTransactionTransport::processError(QSharedPointer<QnTransactionTransport> sibling)
{
    QMutexLocker lock(&mutex);
    closeSocket();
    readSync = false;
    writeSync = false;
    if (isConnectionOriginator) 
        state = State::Connect;
    else
        state = State::Closed;
    dataToSend.clear();

    if (sibling && sibling->getState() == ReadyForStreaming) 
        sibling->processError(QSharedPointer<QnTransactionTransport>());
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
                            setState(State::Error);
                        return; // no more data
                    }
                    readBufferLen += readed;
                    if (chunkHeaderLen == 0)
                        chunkHeaderLen = getChunkHeaderEnd(rBuffer, readBufferLen, &chunkLen);
                    if (chunkHeaderLen) {
                        const int fullChunkLen = chunkHeaderLen + chunkLen + 2;
                        if (readBufferLen == fullChunkLen) {
                            QByteArray dataCopy;
                            dataCopy.append((const char *) rBuffer + chunkHeaderLen, chunkLen);
                            owner->gotTransaction(remoteGuid, isConnectionOriginator, dataCopy);
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
                        setState(State::Error);
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
        setState(State::Error);
        break;
    default:
        break;
    }
}

void QnTransactionTransport::doOutgoingConnect()
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
    setState(ReadyForStreaming);
    chunkHeaderLen = 0;

    const quint8* buffer = (const quint8*) data.data();
    int bufferLen = data.size();
    chunkHeaderLen = getChunkHeaderEnd(buffer, bufferLen, &chunkLen);
    while (chunkHeaderLen >= 0) 
    {
        int fullChunkLen = chunkHeaderLen + chunkLen + 2;
        if (bufferLen >= fullChunkLen)
        {
            QByteArray dataCopy;
            dataCopy.append(QByteArray::fromRawData((const char *) buffer + chunkHeaderLen, chunkLen));
            owner->gotTransaction(remoteGuid, isConnectionOriginator, dataCopy);
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
        setState(State::Error);
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    nx_http::HttpHeaders::const_iterator itr = client->response()->headers.find("guid");
    if (itr != client->response()->headers.end()) {
        remoteGuid = itr->second;
    }
    else {
        setState(State::Error);
        return;
    }

    QByteArray data = httpClient->fetchMessageBodyBuffer();
    if (!data.isEmpty())
        processTransactionData(data);
    socket = httpClient->takeSocket();
    httpClient.reset();

    setState(QnTransactionTransport::Connected);
}

}
