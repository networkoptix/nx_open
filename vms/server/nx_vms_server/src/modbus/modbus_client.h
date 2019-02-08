#pragma once

#include <memory>

#include <nx/network/socket.h>

#include "modbus.h"

namespace nx
{

namespace modbus
{

class QnModbusClient
{
    static const size_t kBufferSize = 1024;

public:
    QnModbusClient();
    QnModbusClient(const nx::network::SocketAddress& sockaddr);
    ~QnModbusClient();

    void setEndpoint(const nx::network::SocketAddress& sockaddr);

    bool connect();
    void disconnect();

    ModbusResponse doModbusRequest(const ModbusRequest&, bool* outStatus);

    // Discrete input is 1-bit read only field.
    ModbusResponse readDiscreteInputs(quint16 startAddress, quint16 inputCount, bool* outStatus);

    // Coil is 1-bit read/write field.
    ModbusResponse readCoils(quint16 startCoilAddress, quint16 coilCount, bool* outStatus);
    ModbusResponse writeCoils(quint16 startCoilAddress, const QByteArray& data, bool *outStatus);
    ModbusResponse writeSingleCoil(quint16 coilAddress, bool coilState, bool* outStatus);

    // Input register is read only word of length 2 byte.
    ModbusResponse readInputRegisters(quint16 startRegister, quint16 registerCount, bool* outStatus);

    // Holding register is read/write word of length 2 byte.
    ModbusResponse readHoldingRegisters(quint16 startRegister, quint16 registerCount, bool* outStatus);
    ModbusResponse writeHoldingRegisters(quint16 startRegister, const QByteArray& data, bool *outStatus);
    ModbusResponse writeSingleHoldingRegister(quint16 registerAddress, const QByteArray& data, bool* outStatus);

private:
    bool initSocket();
    ModbusMBAPHeader buildHeader(const ModbusRequest& request);

private:
    quint16 m_requestTransactionId;
    char m_recvBuffer[kBufferSize];

    nx::network::SocketAddress m_endpoint;
    bool m_connected;
    std::shared_ptr<nx::network::AbstractStreamSocket> m_socket;
};

} //< Closing namespace modbus.

} //< Closing namespace nx.
