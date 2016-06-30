#include "modbus_client.h"
#include "modbus.h"

using namespace nx_modbus;

QnModbusClient::QnModbusClient(const SocketAddress& sockaddr) :
    m_endpoint(sockaddr),
    m_socket(new TCPSocket())
{
}

QnModbusClient::~QnModbusClient()
{
    m_socket->shutdown();
}

bool QnModbusClient::connect()
{
    return m_socket->connect(m_endpoint);
}

ModbusResponse QnModbusClient::doModbusRequest(const ModbusRequest &request)
{
    auto data = ModbusRequest::encode(request);
    m_socket->send(data);
}

ModbusResponse QnModbusClient::readHoldingRegisters(quint16 startRegister, quint16 reqisterCount)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kWriteMultipleRegisters;

    QDataStream stream(&request.data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    stream
        << startRegister
        << registerCount;

    request.header = buildHeader(request);

    return doModbusRequest(request);
}

ModbusResponse QnModbusClient::writeHoldingRegisters(quint16 startRegister, const QByteArray &data)
{
    ModbusRequest request;

    request.functionCode = FunctionCode::kReadHoldingRegisters;
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

    return doModbusRequest(request);
}
