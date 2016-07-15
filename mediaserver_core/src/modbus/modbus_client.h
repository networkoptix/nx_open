#pragma once

#include <memory>

#include <utils/network/system_socket.h>

#include "modbus.h"

namespace nx_modbus
{

class QnModbusClient
{
    static const size_t kBufferSize = 1024;

public:
    QnModbusClient();
    QnModbusClient(const SocketAddress& sockaddr);
    ~QnModbusClient();

    void setEndpoint(const SocketAddress& sockaddr);

    bool connect();
    void disconnect();

    ModbusResponse doModbusRequest(const ModbusRequest&, bool& success);

    ModbusResponse readDiscreteInputs();

    ModbusResponse readCoils();
    ModbusResponse writeCoils();

    ModbusResponse writeSingleCoil(quint16 coilAddress, bool coilState);

    ModbusResponse readInputRegisters();

    ModbusResponse readHoldingRegisters(quint16 startRegister, quint16 registerCount);
    ModbusResponse writeHoldingRegisters(quint16 startRegister, const QByteArray& data);

private:
    bool initSocket();
    ModbusMBAPHeader buildHeader(const ModbusRequest& request);

private:
    quint16 m_requestTransactionId;
    char m_recvBuffer[kBufferSize];

    SocketAddress m_endpoint;
    std::shared_ptr<AbstractStreamSocket> m_socket;
};

}
