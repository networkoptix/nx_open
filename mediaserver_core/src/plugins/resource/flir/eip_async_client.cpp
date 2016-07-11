#include "eip_async_client.h"
#include <utils/common/log.h>

const size_t RECEIVE_BUFFER_SIZE = 1024*50;

EIPAsyncClient::EIPAsyncClient(QHostAddress hostAddress) :
    m_hostAddress(hostAddress),
    m_port(kEipPort),
    m_terminated(false),
    m_inProcess(false),
    m_responseFound(0),
    m_hasPendingRequest(false),
    m_dataLength(0),
    m_currentState(EIPClientState::NeedSession),
    m_eipStatus(EIPStatus::kEipStatusSuccess),
    m_sessionHandle(0)
{
    m_recvBuffer.reserve(RECEIVE_BUFFER_SIZE);
    m_sendBuffer.reserve(RECEIVE_BUFFER_SIZE);
    initSocket();
}

EIPAsyncClient::~EIPAsyncClient()
{
    terminate();
}

void EIPAsyncClient::setPort(quint16 port)
{
    m_port = port;
}

void EIPAsyncClient::terminate()
{
    QnMutexLocker lk(&m_mutex);
    if(m_terminated)
        return;

    m_terminated = true;
    if(m_socket)
        m_socket->cancelAsyncIO();
}

void EIPAsyncClient::initSocket()
{
    m_socket = std::make_shared<TCPSocket>();
    if(!(m_socket->connect(m_hostAddress.toString(), static_cast<unsigned short>(m_port))))
        NX_LOG(lit("Async Ethernet/IP client failed to connect to host: %1:%2")
            .arg(m_hostAddress.toString())
            .arg(kEipPort),
            cl_logDEBUG2);
}

void EIPAsyncClient::asyncSendDone(
    std::shared_ptr<AbstractStreamSocket> socket,
    SystemError::ErrorCode errorCode,
    size_t bytesWritten)
{
    QnMutexLocker lock(&m_mutex);
    if(m_terminated)
        return;

    if(errorCode != SystemError::noError)
    {
        m_currentState = EIPClientState::Error;
        return;
    }

    using namespace std::placeholders;
    m_socket->readSomeAsync(
        &m_recvBuffer,
        std::bind(
            &EIPAsyncClient::onSomeBytesReadAsync, this,
            m_socket, _1, _2));
}

void EIPAsyncClient::onSomeBytesReadAsync(
    std::shared_ptr<AbstractStreamSocket> socket,
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    QnMutexLocker lock(&m_mutex);
    if(m_terminated)
        return;

    if(m_currentState == EIPClientState::WaitingForHeader || m_currentState == EIPClientState::ReadingHeader)
        processHeaderBytes();
    else if(m_currentState == EIPClientState::ReadingData)
        processDataBytes();
    else if(m_currentState == EIPClientState::WaitingForSession)
        processSessionBytes();

    m_recvBuffer.truncate(0);
    processState();
}

void EIPAsyncClient::processHeaderBytes()
{
    auto bytesLeft = EIPEncapsulationHeader::SIZE - m_headerBuffer.size();
    m_headerBuffer.append(m_recvBuffer.left(bytesLeft));
    m_dataBuffer.append(m_recvBuffer.mid(bytesLeft));

    Q_ASSERT(m_headerBuffer.size() <= EIPEncapsulationHeader::SIZE);
    if(m_headerBuffer.size() == EIPEncapsulationHeader::SIZE)
    {
        auto header = EIPPacket::parseHeader(m_headerBuffer);
        if(header.status == EIPStatus::kEipStatusSuccess)
        {
            m_dataLength = header.dataLength;
            m_currentState = EIPClientState::ReadingData;
        }
        else if(header.status == EIPStatus::kEipStatusInvalidSessionHandle)
        {
            m_currentState = EIPClientState::NeedSession;
        }
        else
        {
            NX_LOG(lit("Async Ethernet/IP client got error. EIP error code: %1")
                .arg(header.status),
                cl_logDEBUG2);
            m_currentState = EIPClientState::Error;
        }
    }
    else
    {
        m_currentState = EIPClientState::ReadingHeader;
    }

    if(m_currentState == EIPClientState::ReadingData)
    {
        Q_ASSERT(m_dataLength >= m_dataBuffer.size());
        if(m_dataLength == m_dataBuffer.size())
        {
            m_currentState = EIPClientState::DataWasRead;
        }
    }

    m_recvBuffer.truncate(0);
}

void EIPAsyncClient::processDataBytes()
{
    m_dataBuffer.append(m_recvBuffer);
    if(m_dataBuffer.size() == m_dataLength)
        m_currentState = EIPClientState::DataWasRead;
    else if(m_dataBuffer.size() < m_dataLength)
        m_currentState = EIPClientState::ReadingData;
    else
        m_currentState = EIPClientState::Error;

    m_recvBuffer.truncate(0);
}

void EIPAsyncClient::processSessionBytes()
{
    m_headerBuffer.append(m_recvBuffer.left(EIPEncapsulationHeader::SIZE));
    Q_ASSERT(m_headerBuffer.size() <= EIPEncapsulationHeader::SIZE);
    if(m_headerBuffer.size() < EIPEncapsulationHeader::SIZE)
        return;

    auto header = EIPPacket::parseHeader(m_headerBuffer);
    if(header.status == EIPStatus::kEipStatusSuccess)
    {
        m_sessionHandle = header.sessionHandle;
        m_currentState = EIPClientState::ReadyToRequest;
    }
    else
    {
        NX_LOG(lit("Async Ethernet/IP client failed to obtain session. EIP status: %1")
               .arg(header.status), cl_logDEBUG2);
        m_currentState = EIPClientState::Error;
    }
}

void EIPAsyncClient::processState()
{   
    if(m_currentState == EIPClientState::NeedSession)
    {
        m_headerBuffer.truncate(0);
        registerSessionAsync();
    }
    else if(m_currentState == EIPClientState::ReadyToRequest)
    {
        m_headerBuffer.truncate(0);
        m_dataBuffer.truncate(0);
        if(m_hasPendingRequest)    
        {
            doServiceRequestAsyncInternal(m_pendingRequest);
        }
        else
        {
            m_inProcess = false;
            m_mutex.unlock();
            emit done();
            m_mutex.lock();
        }

    }
    else if(m_currentState == EIPClientState::DataWasRead)
    {
        nx::Buffer messageBuf;
        messageBuf
            .append(m_headerBuffer)
            .append(m_dataBuffer);

        processPacket(messageBuf);
        m_headerBuffer.truncate(0);
        m_dataBuffer.truncate(0);
        m_inProcess = false;
        m_mutex.unlock();
        emit done();
        m_mutex.lock();
    }
    else if(m_currentState == EIPClientState::Error)
    {
        m_headerBuffer.truncate(0);
        m_dataBuffer.truncate(0);
        m_inProcess = false;
        m_mutex.unlock();
        emit done();
        m_mutex.lock();
    }
    else
    {
        using namespace std::placeholders;
        m_socket->readSomeAsync(
            &m_recvBuffer,
            std::bind(&EIPAsyncClient::onSomeBytesReadAsync, this, m_socket, _1, _2));
    }
}

void EIPAsyncClient::processPacket(const nx::Buffer &buf)
{
    MessageRouterResponse response;
    auto header = EIPPacket::parseHeader(
        buf.left(EIPEncapsulationHeader::SIZE));

    m_eipStatus = header.status;
    auto data = buf.mid(
        EIPEncapsulationHeader::SIZE,
        header.dataLength);

    auto cpf = CPFPacket::decode(data.mid(
        sizeof(decltype(EIPEncapsulationData::handle)) + 
        sizeof(decltype(EIPEncapsulationData::timeout))));

    for(const auto& item: cpf.items)
        if(item.typeId == CIPItemID::kUnconnectedItemData)
        {
            m_responseFound = true;
            m_response = MessageRouterResponse::decode(item.data);
        }

    m_currentState = EIPClientState::ReadyToRequest;
}

MessageRouterResponse EIPAsyncClient::getResponse()
{
    return m_response;
}

bool EIPAsyncClient::doServiceRequestAsync(const MessageRouterRequest &request)
{
    if(m_terminated)
        return false;

    QnMutexLocker lock(&m_mutex);

    if(m_inProcess)
        return false;

    m_inProcess = true;
    auto encodedPacket = buildEIPServiceRequest(request);
    m_pendingRequest = request;
    m_sendBuffer.truncate(0);
    m_sendBuffer.append(encodedPacket);

    if(m_currentState == EIPClientState::NeedSession)
    {
        m_hasPendingRequest = true;
        return registerSessionAsync();\
    }
    else
    {
        m_hasPendingRequest = false;
        return doServiceRequestAsyncInternal(request);
    }
}

bool EIPAsyncClient::doServiceRequestAsyncInternal(const MessageRouterRequest &request)
{
    using namespace std::placeholders;
    m_currentState = EIPClientState::WaitingForHeader;
    return m_socket->sendAsync(
        m_sendBuffer,
        std::bind(&EIPAsyncClient::asyncSendDone, this, m_socket, _1, _2));
}

bool EIPAsyncClient::registerSessionAsync()
{
    if(m_terminated)
        return false;

    auto buf = buildEIPRegisterSessionRequest();
    m_sendBuffer.truncate(0);
    m_sendBuffer.append(buf);
    m_currentState = EIPClientState::WaitingForSession;

    using namespace std::placeholders;
    return m_socket->sendAsync(
        m_sendBuffer,
        std::bind(&EIPAsyncClient::asyncSendDone, this, m_socket, _1, _2));
}

void EIPAsyncClient::unregisterSession()
{

}

nx::Buffer EIPAsyncClient::buildEIPServiceRequest(const MessageRouterRequest &request) const
{
    EIPPacket encPacket;
    encPacket.header.commandCode = EIPCommand::kEipCommandSendRRData;
    encPacket.header.sessionHandle = m_sessionHandle;

    CPFPacket cpfPacket;
    CPFItem addressItem;
    CPFItem dataItem;

    addressItem.typeId = CIPItemID::kNullAddress;
    addressItem.dataLength = 0;

    // This field should contain CID and is necessary only for connected messages
    // We use unconnected messages
    QDataStream stream(&addressItem.data, QIODevice::WriteOnly);
    stream << quint32(0x00b000b0);

    auto encodedRequest = MessageRouterRequest::encode(request);

    dataItem.typeId = CIPItemID::kUnconnectedItemData;
    dataItem.dataLength = encodedRequest.size();
    dataItem.data = encodedRequest;

    cpfPacket.itemCount = 2;
    cpfPacket.items.push_back(addressItem);
    cpfPacket.items.push_back(dataItem);

    encPacket.data.handle = 0;
    encPacket.data.timeout = 30;
    encPacket.data.cpfPacket = cpfPacket;

    encPacket.header.status = 0;
    encPacket.header.senderContext = 0;
    encPacket.header.options = 0;

    return EIPPacket::encode(encPacket);
}

nx::Buffer EIPAsyncClient::buildEIPRegisterSessionRequest()
{
    nx::Buffer encoded;
    nx::Buffer buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    EIPEncapsulationHeader encPacketHeader;

    encPacketHeader.commandCode = EIPCommand::kEipCommandRegisterSession;
    encPacketHeader.dataLength = 4;
    encPacketHeader.sessionHandle = 0;
    encPacketHeader.status = 0;
    encPacketHeader.senderContext = 0;
    encPacketHeader.options = 0;

    encoded.append(EIPEncapsulationHeader::encode(encPacketHeader));

    stream << quint16(0x0001) /* Protocol version*/ << quint16(0x0000) /*Options flags*/;
    encoded.append(buf);

    return encoded;
}

