#include "modbus_client.h"
#include "modbus.h"

using namespace nx_modbus;

namespace
{
    const int kDefaultConnectionTimeoutMs = 4000; 
    const int kSendTimeout = 4000;
    const int kReceiveTimeout = 4000;
}

QnModbusClient::QnModbusClient():
    m_requestTransactionId(0),
    m_needReinitSocket(true)
{
}

QnModbusClient::QnModbusClient(const SocketAddress& sockaddr) :
    m_requestTransactionId(0),
    m_endpoint(sockaddr),
    m_needReinitSocket(true)
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
    if (m_socket)
        m_socket->shutdown();

    m_socket.reset(SocketFactory::createStreamSocket(false));

    if (!m_socket->setRecvTimeout(kReceiveTimeout)
        || !m_socket->setSendTimeout(kSendTimeout))
    {
        return false;
    }

    m_needReinitSocket = false;

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

    return m_socket->connect(m_endpoint, kDefaultConnectionTimeoutMs);
}

ModbusResponse QnModbusClient::doModbusRequest(const ModbusRequest &request, bool& success)
{
    ModbusResponse response;

    if (m_endpoint.isNull())
    {
        success = false;
        return response;
    }

    if (!m_socket && !initSocket())
    {
        success = false;
        m_needReinitSocket = true;
        return response;
    }

    if (m_needReinitSocket)
    {
        initSocket();
        if(!m_socket->connect(m_endpoint))
        {
            success = false;
            m_needReinitSocket = true;
            return response;
        }
    }

    success = true;

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
            success = false;
            m_needReinitSocket = true;
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
            success = false;
            m_needReinitSocket = true;
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
        success = false;

    if (success)
        response = ModbusResponse::decode(QByteArray(m_recvBuffer, bytesNeeded));
    else
        m_needReinitSocket = true;

    return response;
}

ModbusResponse QnModbusClient::readHoldingRegisters(quint16 startRegister, quint16 registerCount)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kReadHoldingRegisters;

    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        << startRegister
        << registerCount;

    request.header = buildHeader(request);

    bool status;

    return doModbusRequest(request, status);
}

ModbusResponse QnModbusClient::writeHoldingRegisters(quint16 startRegister, const QByteArray &data)
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

    bool status;

    return doModbusRequest(request, status);
}

ModbusResponse QnModbusClient::writeSingleCoil(quint16 coilAddress, bool coilState, bool& status)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kWriteSingleCoil;
    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream 
        << coilAddress
        << (coilState ? kCoilStateOn : kCoilStateOff);

    request.header = buildHeader(request);

    return doModbusRequest(request, status);
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

ModbusResponse QnModbusClient::writeSingleHoldingRegister(quint16 registerAddres, const QByteArray& data)
{
    ModbusResponse response;

    Q_ASSERT_X(false, "ModbusClient::writeSingleHoldingRegister", "Not implemented");

    return response;
}
