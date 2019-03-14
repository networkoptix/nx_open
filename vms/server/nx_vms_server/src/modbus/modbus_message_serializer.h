#pragma once

#include "modbus.h"

#include <nx/network/connection_server/base_protocol_message_types.h>

namespace nx
{

namespace modbus
{

class ModbusMessageSerializer
{

public:

    ModbusMessageSerializer();

    void setMessage(const ModbusMessage* message);

    nx::network::server::SerializerState serialize(nx::Buffer* const buffer, size_t* const bytesWritten);

    static nx::Buffer serialized(const ModbusMessage& message);

private:
    nx::network::server::SerializerState serializeHeader(nx::Buffer* const buffer);
    nx::network::server::SerializerState serializeData(nx::Buffer* const buffer);

private:
    const ModbusMessage* m_message;
    bool m_initialized;
};

} //< Closing namespace modbus

} //< Closing namespace nx
