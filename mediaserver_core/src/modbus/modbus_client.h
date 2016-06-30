#pragma once
#include "modbus.h"
#include <memory>
#include <utils/network/system_socket.h>

namespace nx_modbus
{

class QnModbusClient
{
public:
    QnModbusClient(const SocketAddress& sockaddr);
    ~QnModbusClient();

    bool connect();

    ModbusResponse doModbusRequest(const ModbusRequest&);

    ModbusResponse readDiscreteInputs();

    ModbusResponse readCoils();
    ModbusResponse writeCoils();

    ModbusResponse readInputRegisters();

    ModbusResponse readHoldingRegisters(quint16 startRegister, quint16 registerCount);
    ModbusResponse writeHoldingRegisters(quint16 startRegister, const QByteArray& data);

private:
    SocketAddress m_endpoint;
    std::shared_ptr<TCPSocket> m_socket;

};

}
