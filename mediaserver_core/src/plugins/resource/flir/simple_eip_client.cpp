#include "simple_eip_client.h"
#include <QtEndian>

#include "eip_cip.h"
#include <nx/network/socket_factory.h>

SimpleEIPClient::SimpleEIPClient(QString addr) :
    m_hostAddress(addr),
    m_port(kDefaultEipPort),
    m_connected(false)
{
}

SimpleEIPClient::~SimpleEIPClient()
{
    unregisterSession();
}

bool SimpleEIPClient::initSocket()
{
    m_connected = false;
    m_eipSocket = nx::network::SocketFactory::createStreamSocket(false);
    bool success = m_eipSocket->setSendTimeout(kDefaultEipTimeout * 1000)
        && m_eipSocket->setRecvTimeout(kDefaultEipTimeout * 1000);

    return success;
}

bool SimpleEIPClient::sendAll(nx::network::AbstractStreamSocket* socket, QByteArray& data)
{
    int totalBytesSent = 0;
    int dataSize = data.size();
    auto rawData = data.data();

    while (totalBytesSent < dataSize)
    {
        auto bytesSent = socket->send(rawData + totalBytesSent, dataSize);
        if (bytesSent <= 0)
        {
            qDebug()
                << "SimpleEIPCLient, error while sending data"
                << SystemError::getLastOSErrorText()
                << SystemError::getLastOSErrorCode();

            return false;
        }

        totalBytesSent += bytesSent;
        dataSize -= bytesSent;
    }

    return true;
}

bool SimpleEIPClient::receiveMessage(nx::network::AbstractStreamSocket* socket, char* const buffer)
{
    int totalBytesRead = 0;

    while (totalBytesRead < EIPEncapsulationHeader::SIZE)
    {
        auto bytesRead = m_eipSocket->recv(
            buffer + totalBytesRead,
            kBufferSize - totalBytesRead);

        if (bytesRead <= 0)
            return false;

        totalBytesRead += bytesRead;
    }

    auto header = EIPPacket::parseHeader(QByteArray(buffer, EIPEncapsulationHeader::SIZE));

    auto totalMesssageLength = header.dataLength + EIPEncapsulationHeader::SIZE;

    while (totalBytesRead < totalMesssageLength)
    {
        auto bytesRead = m_eipSocket->recv(
            buffer + totalBytesRead,
            kBufferSize - totalBytesRead);

        if (bytesRead <= 0)
            return false;

        totalBytesRead += bytesRead;
    }

    return true;
}

void SimpleEIPClient::handleSocketError()
{
    m_connected = false;
    m_eipSocket.reset();
}

void SimpleEIPClient::setPort(const quint16 port)
{
    m_port = port;
}

MessageRouterRequest SimpleEIPClient::buildMessageRouterRequest(
    const quint8 serviceId,
    const quint8 classId,
    const quint8 instanceId,
    const quint8 attributeId,
    const QByteArray &data) const
{
    MessageRouterRequest request;
    request.serviceCode = serviceId;
    request.pathSize = attributeId == 0 ? 2 : 3;
    request.epath[0] =
        CIPSegmentType::kLogicalSegment |
        CIPSegmentLogicalType::kClassId |
        CIPSegmentLogicalFormat::kBit8;
    request.epath[1] = classId;
    request.epath[2] =
        CIPSegmentType::kLogicalSegment |
        CIPSegmentLogicalType::kInstanceId |
        CIPSegmentLogicalFormat::kBit8;
    request.epath[3] = instanceId;
    if(attributeId != 0)
    {
        request.epath[4] =
            CIPSegmentType::kLogicalSegment |
            CIPSegmentLogicalType::kAttributeId |
            CIPSegmentLogicalFormat::kBit8;
        request.epath[5] = attributeId;
    }
    request.data = data;

    return request;
}

EIPPacket SimpleEIPClient::buildEIPEncapsulatedPacket(
    const MessageRouterRequest &request) const
{

    EIPPacket encPacket;
    encPacket.header.commandCode = EIPCommand::kEipCommandSendRRData;
    encPacket.header.sessionHandle = m_sessionHandle;

    CPFPacket cpfPacket;
    CPFItem addressItem;
    CPFItem dataItem;

    addressItem.typeId = CIPItemID::kNullAddress;
    addressItem.dataLength = 0x0000;

    // This field should contain CID and is necessary only for connected messages
    // We use unconnected messages
    QDataStream stream(&addressItem.data, QIODevice::WriteOnly);
    stream << quint32(0x00b000b0);

    auto encodedRequest = MessageRouterRequest::encode(request);

    dataItem.typeId = CIPItemID::kUnconnectedItemData;
    dataItem.dataLength = encodedRequest.size();
    dataItem.data = encodedRequest;

    cpfPacket.itemCount = 0x0002;
    cpfPacket.items.push_back(addressItem);
    cpfPacket.items.push_back(dataItem);

    encPacket.data.handle = 0;
    encPacket.data.timeout = kDefaultEipTimeout;
    encPacket.data.cpfPacket = cpfPacket;

    encPacket.header.status = 0;
    encPacket.header.senderContext = 0;
    encPacket.header.options = 0;

    return encPacket;
}

MessageRouterResponse SimpleEIPClient::getServiceResponseData(const QByteArray& buf) const
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
        if(item.typeId == CIPItemID::kUnconnectedItemData)
            return MessageRouterResponse::decode(item.data);

    return response;
}

bool SimpleEIPClient::tryGetResponse(const MessageRouterRequest &request, QByteArray &data, eip_status_t* outStatus)
{
    auto encPacket = buildEIPEncapsulatedPacket(request);
    auto encodedEncapPacket = EIPPacket::encode(encPacket);

    auto success = sendAll(m_eipSocket.get(), encodedEncapPacket);
    if (!success)
    {
        handleSocketError();
        return false;
    }

    success = receiveMessage(m_eipSocket.get(), m_recvBuffer);
    if (!success)
    {
        handleSocketError();
        return false;
    }

    auto header = EIPPacket::parseHeader(QByteArray(m_recvBuffer, EIPEncapsulationHeader::SIZE));
    data = QByteArray(m_recvBuffer, EIPEncapsulationHeader::SIZE + header.dataLength);

    if (outStatus)
        *outStatus = header.status;

    return true;

}

MessageRouterResponse SimpleEIPClient::doServiceRequest(
    quint8 serviceId,
    quint8 classId,
    quint8 instanceId,
    quint8 attributeId,
    const QByteArray& data)
{
    auto request = buildMessageRouterRequest(
        serviceId,
        classId,
        instanceId,
        attributeId,
        data);

    return doServiceRequest(request);
}

MessageRouterResponse SimpleEIPClient::doServiceRequest(const MessageRouterRequest &request)
{
    QnMutexLocker lock(&m_mutex);
    QByteArray response;
    eip_status_t status;

    if (!connectIfNeeded())
        return MessageRouterResponse();

    bool success = tryGetResponse(request, response, &status);
    if (!success)
        return MessageRouterResponse();

    if(status == EIPStatus::kEipStatusInvalidSessionHandle)
    {
        registerSessionUnsafe();
        tryGetResponse(request, response, &status);
    }

    return getServiceResponseData(response);
}

bool SimpleEIPClient::connectIfNeeded()
{
    if (m_connected)
        return true;

    if (!m_eipSocket && !initSocket())
        return false;

    m_connected = m_eipSocket->connect(
        m_hostAddress, m_port,
        std::chrono::milliseconds(kDefaultEipTimeout * 1000));

    if (!m_connected)
        m_eipSocket.reset();

    return m_connected;
}

bool SimpleEIPClient::registerSession()
{
    QnMutexLocker lock(&m_mutex);
    return registerSessionUnsafe();
}

bool SimpleEIPClient::registerSessionUnsafe()
{
    if (!connectIfNeeded())
    {
        handleSocketError();
        return false;
    }

    QByteArray encoded;
    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    EIPEncapsulationHeader encPacketHeader;

    encPacketHeader.commandCode = EIPCommand::kEipCommandRegisterSession;
    encPacketHeader.dataLength = 0x0004;
    encPacketHeader.sessionHandle = 0;
    encPacketHeader.status = 0;
    encPacketHeader.senderContext = 0x01;
    encPacketHeader.options = 0;

    encoded.append(EIPEncapsulationHeader::encode(encPacketHeader));

    const quint16 protocolVersion(0x0001);
    const quint16 optionFlags(0);

    stream << protocolVersion << optionFlags;
    encoded.append(buf);

    bool success = sendAll(m_eipSocket.get(), encoded);
    if (!success)
    {
        handleSocketError();
        return false;
    }

    success = receiveMessage(m_eipSocket.get(), m_recvBuffer);
    if (!success)
    {
        handleSocketError();
        return false;
    }

    encPacketHeader = EIPPacket::parseHeader(QByteArray(m_recvBuffer, EIPEncapsulationHeader::SIZE));

    if(encPacketHeader.status != EIPStatus::kEipStatusSuccess)
    {
        qDebug() << "Sync Ethernet/IP client session registration error:" << encPacketHeader.status;
        return false;
    }

    m_sessionHandle = encPacketHeader.sessionHandle;
    return true;
}

bool SimpleEIPClient::unregisterSession()
{
    QnMutexLocker lock(&m_mutex);

    if(!m_sessionHandle)
        return false;

    if (!connectIfNeeded())
    {
        handleSocketError();
        return false;
    }

    QByteArray encoded;

    EIPEncapsulationHeader encHeader;
    encHeader.commandCode = EIPCommand::kEipCommandUnregisterSession;
    encHeader.dataLength = 0;
    encHeader.sessionHandle = m_sessionHandle;
    encHeader.status = 0;
    encHeader.senderContext = 0;
    encHeader.options = 0;

    auto encodedPacket = EIPEncapsulationHeader::encode(encHeader);

    auto success = sendAll(m_eipSocket.get(), encoded);
    if (!success)
    {
        handleSocketError();
        return false;
    }

    m_sessionHandle = 0;

    return true;
}
