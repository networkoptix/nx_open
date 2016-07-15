#include "modbus_client.h"
#include "modbus.h"

using namespace nx_modbus;

namespace
{
    const int kDefaultConnectionTimeoutMs = 4000; 
}

QnModbusClient::QnModbusClient():
    m_requestTransactionId(0)
{

}

QnModbusClient::QnModbusClient(const SocketAddress& sockaddr) :
    m_requestTransactionId(0),
    m_endpoint(sockaddr)
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
    m_socket->setRecvTimeout(10000);

    // TODO: #dmishin add some checks here. 
    return true;
}

void QnModbusClient::setEndpoint(const SocketAddress& endpoint)
{
    m_endpoint = endpoint;
    initSocket();
}

bool QnModbusClient::connect()
{
    if (!m_socket)
        initSocket();

    return m_socket->connect(m_endpoint, kDefaultConnectionTimeoutMs);
}

ModbusResponse QnModbusClient::doModbusRequest(const ModbusRequest &request, bool& success)
{
    ModbusResponse response;

    if (!m_socket)
    {
        success = false;
        return response;
    }

    if (!m_socket->isConnected())
        m_socket->connect(m_endpoint);

    success = true;

    auto data = ModbusRequest::encode(request);
    auto bytesSent = m_socket->send(data.constData(), data.size());

    if (bytesSent < 1)
        success = false;

    auto bytesRead = m_socket->recv(m_recvBuffer, kBufferSize);

    if (bytesRead < 0)
        success = false;

    if (success)
        response = ModbusResponse::decode(QByteArray(m_recvBuffer, bytesRead));

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

ModbusResponse QnModbusClient::writeSingleCoil(quint16 coilAddress, bool coilState)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kWriteSingleCoil;
    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream 
        << coilAddress
        << (coilState ? kCoilStateOn : kCoilStateOff);

    request.header = buildHeader(request);

    bool status;

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
