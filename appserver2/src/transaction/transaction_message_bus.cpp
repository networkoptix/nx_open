#include "transaction_message_bus.h"
#include "remote_ec_connection.h"
#include "utils/network/aio/aioservice.h"
#include "utils/common/systemerror.h"

namespace ec2
{

// --------------------------------- QnTransactionTransport ------------------------------

void QnTransactionTransport::ensureSize(std::vector<quint8>& buffer, int size)
{
    if (buffer.size() < size)
        buffer.resize(size);
}

int QnTransactionTransport::chunkHeaderEnd(quint32* size)
{
    const quint8* data = &readBuffer[0];
    size = 0;
    for (int i = 0; i < readBufferLen - 1; ++i)
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

void QnTransactionTransport::processError()
{
    socket->close();
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
            if (state == ReadChunkHeader) 
            {
                ensureSize(readBuffer, 12);
                quint8* rBuffer = &readBuffer[0];
                readed = socket->recv(rBuffer + readBufferLen, 4);
                if (readed < 1) {
                    // no more data or error
                    if(readed == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock)
                        processError();
                    return; // no more data
                }
                readBufferLen += readed;
                int chunkEnd = chunkHeaderEnd(&chunkLen);
                if (chunkEnd >= 0) {
                    memmove(&readBuffer[0], rBuffer + chunkEnd, readBufferLen - chunkEnd); // 3 bytes max
                    readBufferLen -= chunkEnd;
                    state = ReadChunkBody;
                }
            }
            else if (state == ReadChunkBody) {
                ensureSize(readBuffer, chunkLen+2);
                quint8* rBuffer = &readBuffer[0];
                readed = socket->recv(rBuffer + readBufferLen, chunkLen+2-readBufferLen);

                if (readed < 1) {
                    // no more data or error
                    if(readed == 0 || SystemError::getLastOSErrorCode() != SystemError::wouldBlock)
                        processError();
                    return; // no more data
                }

                readBufferLen += readed;
                if (readBufferLen == chunkLen + 2)
                {
                    owner->gotTransaction(QByteArray::fromRawData((const char *) rBuffer, chunkLen));
                    state = ReadChunkHeader;
                    readBufferLen = 0;
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
        while (!dataToSend.isEmpty())
        {
            QByteArray& data = dataToSend.front();
            const char* dataStart = data.data();
            const char* dataEnd = dataStart + data.size();
            int sended = socket->send(dataStart + sendOffset, data.size() - sendOffset);
            if (sended < 1)
                return; // can't send any more
            sendOffset + sended;
            if (sendOffset == data.size())
                dataToSend.dequeue();
        }
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
    httpClient = nx_http::AsyncHttpClientPtr(new nx_http::AsyncHttpClient());
    connect(httpClient.get(), &nx_http::AsyncHttpClient::responseReceived, this, &QnTransactionTransport::at_responseReceived);
    connect(httpClient.get(), &nx_http::AsyncHttpClient::done, this, &QnTransactionTransport::at_httpClientDone);
}

void QnTransactionTransport::at_httpClientDone(nx_http::AsyncHttpClientPtr client)
{
    // todo: implement me
    int gg = 4;
}

void QnTransactionTransport::at_responseReceived(nx_http::AsyncHttpClientPtr client)
{
    socket = httpClient->takeSocket();
    httpClient.reset();
    state = QnTransactionTransport::ReadChunkHeader;

    aio::AIOService::instance()->watchSocket( socket, PollSet::etRead, this );
    aio::AIOService::instance()->watchSocket( socket, PollSet::etWrite, this );
}

// --------------------------------- QnTransactionMessageBus ------------------------------

static QnTransactionMessageBus*m_globalInstance = 0;

QnTransactionMessageBus* QnTransactionMessageBus::instance()
{
    return m_globalInstance;
}
void QnTransactionMessageBus::initStaticInstance(QnTransactionMessageBus* instance)
{
    Q_ASSERT(m_globalInstance == 0);
    m_globalInstance = instance;
}

template <class T>
template <class T2>
bool QnTransactionMessageBus::CustomHandler<T>::deliveryTransaction(ApiCommand::Value command, InputBinaryStream<QByteArray>& stream)
{
    QnTransaction<T2> tran;
    if (!tran.deserialize(command, &stream))
        return false;
    m_handler->processTransaction<T2>(tran);
    return true;
}

template <class T>
bool QnTransactionMessageBus::CustomHandler<T>::processByteArray(QByteArray& data)
{
    Q_ASSERT(data.size() > 4);
    InputBinaryStream<QByteArray> stream(data);
    ApiCommand::Value command;
    if (!QnBinary::deserialize(command, &stream))
        return false; // bad data
    switch (command)
    {
        case ApiCommand::addCamera:
        case ApiCommand::updateCamera:
            deliveryTransaction<ApiCameraData>(command, stream);
            break;
            
        default:
            break;
    }

    return true;
}

void QnTransactionMessageBus::run()
{
    for (int i = m_connections.size()-1; i >= 0; --i)
    {
        QnTransactionTransport* transport = m_connections[i];
        switch (transport->state) 
        {
            case QnTransactionTransport::Connect:
            {
                transport->doClientConnect();
                break;
            }
            case QnTransactionTransport::Closed:
                delete m_connections[i];
                m_connections.removeAt(i);
                break;
        }
    }
}

void QnTransactionMessageBus::gotConnectionFromRemotePeer(QSharedPointer<AbstractStreamSocket> socket)
{
    QnTransactionTransport* transport = new QnTransactionTransport();
    transport->isClientSide = false;
    transport->socket = socket;
    transport->state = QnTransactionTransport::ReadChunkHeader;
    m_connections.push_back(transport);
}

void QnTransactionMessageBus::addConnectionToPeer(const QUrl& url)
{
    QnTransactionTransport* transport = new QnTransactionTransport();
    transport->isClientSide = true;
    transport->remoteAddr = url;
    transport->state = QnTransactionTransport::Connect;
    m_connections.push_back(transport);
}

template class QnTransactionMessageBus::CustomHandler<RemoteEC2Connection>;

}
