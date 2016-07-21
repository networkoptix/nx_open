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

    // Discrete input is 1-bit read only field.
    ModbusResponse readDiscreteInputs();

    // Coil is 1-bit read/write field.
    ModbusResponse readCoils(quint16 startCoilAddress, quint16 coilCount);
    ModbusResponse writeCoils(quint16 startCoilAddress, const QByteArray& data);
    ModbusResponse writeSingleCoil(quint16 coilAddress, bool coilState);

    // Input register is read only word of length 2 byte.
    ModbusResponse readInputRegisters(quint16 startRegister, quint16 registerCount);

    // Holding register is read/write word of length 2 byte.
    ModbusResponse readHoldingRegisters(quint16 startRegister, quint16 registerCount);
    ModbusResponse writeHoldingRegisters(quint16 startRegister, const QByteArray& data);
    ModbusResponse writeSingleHoldingRegister(quint16 registerAddres, const QByteArray& data);

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
