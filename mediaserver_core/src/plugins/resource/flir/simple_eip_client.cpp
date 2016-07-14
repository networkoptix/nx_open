#include "simple_eip_client.h"
#include <QtEndian>



SimpleEIPClient::SimpleEIPClient(QHostAddress addr) :
    m_hostAddress(addr),
    m_port(kDefaultEipPort)
{
    initSocket();
}

SimpleEIPClient::~SimpleEIPClient()
{
    unregisterSession();
}

void SimpleEIPClient::initSocket()
{
    m_eipSocket = TCPSocketPtr(SocketFactory::createStreamSocket(false));
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

eip_status_t SimpleEIPClient::tryGetResponse(const MessageRouterRequest &request, QByteArray &data)
{
    auto encPacket = buildEIPEncapsulatedPacket(request);
    auto encodedEncapPacket = EIPPacket::encode(encPacket);

    m_eipSocket->send(encodedEncapPacket);

    auto bytesRead =  m_eipSocket->recv(m_recvBuffer, kBufferSize);
    auto header = EIPPacket::parseHeader(QByteArray(m_recvBuffer, EIPEncapsulationHeader::SIZE));
    data = QByteArray(m_recvBuffer, bytesRead);

    return header.status;
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
    auto status = tryGetResponse(request, response);
    if(status == EIPStatus::kEipStatusInvalidSessionHandle)
    {
        lock.unlock();
        registerSession();
        lock.relock();
        tryGetResponse(request, response);
    }

    return getServiceResponseData(response);
}

bool SimpleEIPClient::connect()
{
    QnMutexLocker lock(&m_mutex);
    return m_eipSocket->connect(m_hostAddress.toString(), m_port);
}

bool SimpleEIPClient::registerSession()
{
    QnMutexLocker lock(&m_mutex);

    QByteArray encoded;
    QByteArray buf;
    QDataStream stream(&buf, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    EIPEncapsulationHeader encPacketHeader;

    encPacketHeader.commandCode = EIPCommand::kEipCommandRegisterSession;
    encPacketHeader.dataLength = 0x0004;
    encPacketHeader.sessionHandle = 0;
    encPacketHeader.status = 0;
    encPacketHeader.senderContext = 0;
    encPacketHeader.options = 0;

    encoded.append(EIPEncapsulationHeader::encode(encPacketHeader));

    const quint16 protocolVersion(0x0001);
    const quint16 optionFlags(0);

    stream << protocolVersion << optionFlags;
    encoded.append(buf);

    m_eipSocket->send(encoded);
    auto bytesRead = m_eipSocket->recv(m_recvBuffer, kBufferSize);

    QByteArray response(m_recvBuffer, bytesRead);
    encPacketHeader = EIPPacket::parseHeader(response);

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

    QByteArray encoded;

    EIPEncapsulationHeader encHeader;
    encHeader.commandCode = EIPCommand::kEipCommandUnregisterSession;
    encHeader.dataLength = 0;
    encHeader.sessionHandle = m_sessionHandle;
    encHeader.status = 0;
    encHeader.senderContext = 0;
    encHeader.options = 0;

    auto encodedPacket = EIPEncapsulationHeader::encode(encHeader);

    m_eipSocket->send(encodedPacket);
    m_sessionHandle = 0;

    return true;
}
