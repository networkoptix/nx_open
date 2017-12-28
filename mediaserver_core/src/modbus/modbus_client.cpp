#include "modbus_client.h"
#include "modbus.h"

namespace
{
static const std::chrono::seconds kDefaultConnectionTimeout(4);
const int kSendTimeout = 4000;
const int kReceiveTimeout = 4000;
}

namespace nx
{

namespace modbus
{

QnModbusClient::QnModbusClient():
    m_requestTransactionId(0),
    m_connected(false)
{
}

QnModbusClient::QnModbusClient(const SocketAddress& sockaddr) :
    m_requestTransactionId(0),
    m_endpoint(sockaddr),
    m_connected(false)
{
    initSocket();
}

QnModbusClient::~QnModbusClient()
{
    if (m_socket)
        m_socket->shutdown();
}

bool QnModbusClient::initSocket()
{
    m_connected = false;

    if (m_socket)
        m_socket->shutdown();

    m_socket = SocketFactory::createStreamSocket(false);

    if (!m_socket->setRecvTimeout(kReceiveTimeout)
        || !m_socket->setSendTimeout(kSendTimeout))
    {
        return false;
    }

    return true;
}

void QnModbusClient::setEndpoint(const SocketAddress& endpoint)
{
    m_endpoint = endpoint;
    initSocket();
}

bool QnModbusClient::connect()
{
    if (!m_socket && !initSocket())
        return false;

    m_connected = m_socket->connect(m_endpoint, kDefaultConnectionTimeout);

    return m_connected;
}

ModbusResponse QnModbusClient::doModbusRequest(const ModbusRequest &request, bool* outStatus)
{
    ModbusResponse response;

    if (m_endpoint.isNull())
    {
        *outStatus = false;
        return response;
    }

    if (!m_socket && !initSocket())
    {
        *outStatus = false;
        return response;
    }

    if (!m_connected)
    {
        initSocket();
        if (!connect())
        {
            *outStatus = false;
            return response;
        }

        m_connected = true;
    }

    *outStatus = true;

    auto data = ModbusRequest::encode(request);
    auto bytesSent = m_socket->send(data.constData(), data.size());
    int totalBytesSent = 0;

    while (totalBytesSent < data.size())
    {
        bytesSent = m_socket->send(
            data.constData() + totalBytesSent,
            data.size() - totalBytesSent);

        if (bytesSent < 1)
        {
            initSocket();
            *outStatus = false;
            return response;
        }
        totalBytesSent += bytesSent;
    }

    auto totalBytesRead = 0;
    auto bytesRead = 0;
    auto bytesNeeded = kModbusMaxMessageLength;

    while (true)
    {
        bytesRead = m_socket->recv(m_recvBuffer + totalBytesRead, kBufferSize - totalBytesRead);
        if (bytesRead <= 0)
        {
            initSocket();
            *outStatus = false;
            return response;
        }

        totalBytesRead += bytesRead;

        if (totalBytesRead >= bytesNeeded)
            break;

        if (totalBytesRead >= ModbusMBAPHeader::size)
        {
            auto header = ModbusMBAPHeader::decode(
                QByteArray(m_recvBuffer, ModbusMBAPHeader::size));

            bytesNeeded = header.length
                + sizeof(decltype(ModbusMBAPHeader::transactionId))
                + sizeof(decltype(ModbusMBAPHeader::protocolId))
                + sizeof(decltype(ModbusMBAPHeader::length));
        }
    }

    if (bytesRead < 0)
        *outStatus = false;

    if (*outStatus)
        response = ModbusResponse::decode(QByteArray(m_recvBuffer, bytesNeeded));
    else
        initSocket();

    return response;
}

ModbusResponse QnModbusClient::readHoldingRegisters(
    quint16 startRegister,
    quint16 registerCount,
    bool* outStatus)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kReadHoldingRegisters;

    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        << startRegister
        << registerCount;

    request.header = buildHeader(request);

    return doModbusRequest(request, outStatus);
}

ModbusResponse QnModbusClient::writeHoldingRegisters(quint16 startRegister, const QByteArray &data, bool* outStatus)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kWriteMultipleRegisters;
    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    quint8 byteCount = data.size();
    quint16 registerCount = byteCount / 2;

    stream
        << startRegister
        << registerCount
        << byteCount;

    request.data.append(data);
    request.header = buildHeader(request);

    return doModbusRequest(request, outStatus);
}

ModbusResponse QnModbusClient::writeSingleCoil(quint16 coilAddress, bool coilState, bool* outStatus)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kWriteSingleCoil;
    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        << coilAddress
        << (coilState ? kCoilStateOn : kCoilStateOff);

    request.header = buildHeader(request);

    return doModbusRequest(request, outStatus);
}

ModbusMBAPHeader QnModbusClient::buildHeader(const ModbusRequest& request)
{
    ModbusMBAPHeader header;

    header.transactionId = ++m_requestTransactionId;
    header.protocolId = 0x00;
    header.unitId = 0x01;
    header.length = sizeof(header.unitId)
        + sizeof(request.functionCode)
        + request.data.size();

    return header;
}

void QnModbusClient::disconnect()
{
    if (m_socket)
    {
        m_socket->shutdown();
        m_socket.reset();
    }
}

ModbusResponse QnModbusClient::writeSingleHoldingRegister(
    quint16 registerAddress,
    const QByteArray& data,
    bool* outStatus)
{
    ModbusResponse response;

    Q_ASSERT_X(false, "ModbusClient::writeSingleHoldingRegister", "Not implemented");

    return response;
}

ModbusResponse QnModbusClient::readDiscreteInputs(quint16 startAddress, quint16 inputCount, bool* outStatus)
{
    NX_ASSERT(false, "QnModbusClient::readDiscreteInputs not implemented.");

    *outStatus = false;
    return ModbusResponse();
}

ModbusResponse QnModbusClient::readCoils(
    quint16 startCoilAddress,
    quint16 coilCount,
    bool* outStatus)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kReadCoils;
    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        << startCoilAddress
        << coilCount;

    request.header = buildHeader(request);

    return doModbusRequest(request, outStatus);
}

ModbusResponse QnModbusClient::writeCoils(quint16 startCoilAddress, const QByteArray& data, bool *outStatus)
{
    NX_ASSERT(false, "QnModbusClient::writeCoils not implemented.");

    *outStatus = false;
    return ModbusResponse();
}

ModbusResponse QnModbusClient::readInputRegisters(quint16 startRegister, quint16 registerCount, bool* outStatus)
{
    NX_ASSERT(false, "QnModbusClient::readInputRegisters not implemented.");

    *outStatus = false;
    return ModbusResponse();
}

} //< Closing namespace modbus.

} //< Closing namespace nx.
