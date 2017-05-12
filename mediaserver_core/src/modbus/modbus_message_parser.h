#pragma once

#include "modbus.h"

#include <nx/network/connection_server/base_protocol_message_types.h>

namespace nx
{
namespace modbus
{

class ModbusMessageParser
{
    enum class State
    {
        initial,
        parsingHeader,
        parsingData,
        done,
        failed
    };

public:
    ModbusMessageParser();

    void setMessage(ModbusMessage* const msg);

    nx::network::server::ParserState parse(const nx::Buffer& buf, size_t* bytesProcessed);

    nx::network::server::ParserState state() const;

    void reset();

private:
    State parseHeader(const nx::Buffer& buf, size_t* bytesProcessed);
    State parseData(const nx::Buffer& buf, size_t* bytesProcessed);

private:
    State m_state;
    ModbusMessage* m_outputMessage;
    std::size_t m_messageDataLength;

};

} //< Closing namespace modbus

} //< Cosing namespace nx

