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

    nx_api::SerializerState::Type serialize(nx::Buffer* const buffer, size_t* const bytesWritten);

    static nx::Buffer serialized(const ModbusMessage& message);

private:
    nx_api::SerializerState::Type serializeHeader(nx::Buffer* const buffer);
    nx_api::SerializerState::Type serializeData(nx::Buffer* const buffer);

private:
    const ModbusMessage* m_message;
    bool m_initialized;
};

} //< Closing namespace modbus

} //< Closing namespace nx
