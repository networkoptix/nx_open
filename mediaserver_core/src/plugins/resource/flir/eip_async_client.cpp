#include "eip_async_client.h"
#include <nx/utils/log/log.h>
#include <chrono>

namespace {

const size_t kReceiveBufferSize = 1024*50;
const std::chrono::milliseconds kSendTimeout = std::chrono::seconds(4);
const std::chrono::milliseconds kReceiveTimeout = std::chrono::seconds(4);

} //namespace

EIPAsyncClient::EIPAsyncClient(QString hostAddress) :
    m_hostAddress(hostAddress),
    m_port(kDefaultEipPort),
    m_terminated(false),
    m_inProcess(false),
    m_hasPendingRequest(false),
    m_dataLength(0),
    m_currentState(EIPClientState::NeedSession),
    m_sessionHandle(0)
{
    m_recvBuffer.reserve(kReceiveBufferSize);
    m_sendBuffer.reserve(kReceiveBufferSize);
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
    if (m_socket)
        m_socket->pleaseStopSync();
}

bool EIPAsyncClient::initSocket()
{
    m_socket = nx::network::SocketFactory::createStreamSocket();
    bool success = m_socket->setNonBlockingMode(true)
        && m_socket->setSendTimeout(kSendTimeout.count())
        && m_socket->setRecvTimeout(kReceiveTimeout.count());

    m_currentState = success ? EIPClientState::NeedSession : EIPClientState::Error;

    if (!success)
        m_socket.reset();

    return success;
}

void EIPAsyncClient::asyncConnectDone(SystemError::ErrorCode errorCode)
{
    QnMutexLocker lock(&m_mutex);
    if (errorCode != SystemError::noError)
        m_currentState = EIPClientState::Error;
    
    processState();
}

void EIPAsyncClient::asyncSendDone(
    SystemError::ErrorCode errorCode,
    size_t bytesWritten)
{
    QnMutexLocker lock(&m_mutex);
    if(m_terminated)
        return;

    if(errorCode != SystemError::noError)
    {
        m_currentState = EIPClientState::Error;
        processState();
        return;
    }

    m_socket->readSomeAsync(
        &m_recvBuffer,
        std::bind(
            &EIPAsyncClient::onSomeBytesReadAsync, this,
            std::placeholders::_1, std::placeholders::_2));
}

void EIPAsyncClient::onSomeBytesReadAsync(
    SystemError::ErrorCode errorCode,
    size_t bytesRead)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return;

    if (errorCode != SystemError::noError)
        m_currentState = EIPClientState::Error;

    if(m_currentState == EIPClientState::ReadingHeader)
        processHeaderBytes();
    else if(m_currentState == EIPClientState::ReadingData)
        processDataBytes();
    else if(m_currentState == EIPClientState::WaitingForSession)
        processSessionBytes();

    processState();
}

void EIPAsyncClient::processHeaderBytes()
{
    if (m_recvBuffer.size() < EIPEncapsulationHeader::SIZE)
        return;

    auto header = EIPPacket::parseHeader(m_recvBuffer);
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
    
    if(m_currentState == EIPClientState::ReadingData)
    {
        Q_ASSERT(m_dataLength + EIPEncapsulationHeader::SIZE >= m_recvBuffer.size());
        if(m_dataLength + EIPEncapsulationHeader::SIZE == m_recvBuffer.size())
            m_currentState = EIPClientState::DataWasRead;
        
    }
}

void EIPAsyncClient::processDataBytes()
{
    Q_ASSERT(m_recvBuffer.size() <= m_dataLength + EIPEncapsulationHeader::SIZE);

    if (m_recvBuffer.size() < m_dataLength + EIPEncapsulationHeader::SIZE)
        return;

    m_currentState = EIPClientState::DataWasRead;
}

void EIPAsyncClient::processSessionBytes()
{
    if(m_recvBuffer.size() < EIPEncapsulationHeader::SIZE)
        return;

    auto header = EIPPacket::parseHeader(m_recvBuffer);
    if (m_recvBuffer.size() < header.dataLength + EIPEncapsulationHeader::SIZE)
        return;

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

void EIPAsyncClient::resetBuffers()
{
    m_sendBuffer.truncate(0);
    m_recvBuffer.truncate(0);

    // I don't see obvious way to preserve buffer capacity,
    // so if it is lesser than some constant just reserve it again. 
    if (m_recvBuffer.capacity() < kReceiveBufferSize)
        m_recvBuffer.reserve(kReceiveBufferSize);

    m_dataLength = 0;
}

void EIPAsyncClient::processState()
{   
    switch (m_currentState)
    {
        case EIPClientState::NeedSession:
            resetBuffers();
            registerSessionAsync();
            break;
        case EIPClientState::ReadyToRequest:
            resetBuffers();
            if(m_hasPendingRequest)
                doServiceRequestAsyncInternal(m_pendingRequest);
            break;
        case EIPClientState::DataWasRead:
            processPacket(m_recvBuffer);
            m_inProcess = false;
            m_currentState = EIPClientState::ReadyToRequest;

            m_mutex.unlock();
            emit done();
            m_mutex.lock();
            break;
        case EIPClientState::Error:
            resetBuffers();
            m_socket.reset();
            m_inProcess = false;

            m_mutex.unlock();
            emit done();
            m_mutex.lock();
            break;
        default:
            m_socket->readSomeAsync(
                &m_recvBuffer,
                std::bind(
                    &EIPAsyncClient::onSomeBytesReadAsync, this,
                    std::placeholders::_1, std::placeholders::_2));
    }
}

void EIPAsyncClient::processPacket(const nx::Buffer &buf)
{
    MessageRouterResponse response;
    auto header = EIPPacket::parseHeader(
        buf.left(EIPEncapsulationHeader::SIZE));

    auto data = buf.mid(
        EIPEncapsulationHeader::SIZE,
        header.dataLength);

    auto cpf = CPFPacket::decode(data.mid(
        sizeof(decltype(EIPEncapsulationData::handle)) +
        sizeof(decltype(EIPEncapsulationData::timeout))));

    for(const auto& item: cpf.items)
    {
        if(item.typeId == CIPItemID::kUnconnectedItemData)
            m_response = MessageRouterResponse::decode(item.data);
    }
}

MessageRouterResponse EIPAsyncClient::getResponse()
{
    return m_response;
}

bool EIPAsyncClient::doServiceRequestAsync(const MessageRouterRequest &request)
{
    QnMutexLocker lock(&m_mutex);

    if (m_terminated)
        return false;

    if (m_inProcess)
        return false;

    if (!m_socket || m_currentState == EIPClientState::Error)
    {
        if (!initSocket())
            return false;
    }

    m_inProcess = true;
    m_pendingRequest = request;

    if (m_currentState == EIPClientState::NeedSession)
    {
        m_hasPendingRequest = true;
        m_socket->connectAsync(
            nx::network::SocketAddress(m_hostAddress, m_port),
            [this](SystemError::ErrorCode c) { asyncConnectDone(c); });

        return true;
    }
    
    return doServiceRequestAsyncInternal(m_pendingRequest);
}

bool EIPAsyncClient::doServiceRequestAsyncInternal(const MessageRouterRequest &request)
{
    resetBuffers();
    m_currentState = EIPClientState::ReadingHeader;
    m_sendBuffer.append(buildEIPServiceRequest(request));

    m_socket->sendAsync(
        m_sendBuffer,
        [this](SystemError::ErrorCode c, size_t b) { asyncSendDone(c, b); });

    return true;
}

bool EIPAsyncClient::registerSessionAsync()
{
    if(m_terminated)
        return false;

    auto buf = buildEIPRegisterSessionRequest();
    m_sendBuffer.append(buf);
    m_currentState = EIPClientState::WaitingForSession;

    m_socket->sendAsync(
        m_sendBuffer,
        [this](SystemError::ErrorCode c, size_t b) { asyncSendDone(c, b); });

    return true;
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

