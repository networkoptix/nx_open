#pragma once

#include "modbus.h"

#include <nx/network/connection_server/base_protocol_message_types.h>

namespace nx_modbus
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

    nx_api::ParserState parse(const nx::Buffer& buf, size_t* bytesProcessed);

    nx_api::ParserState state() const;

    void reset();

private:
    State parseHeader(const nx::Buffer& buf, size_t* bytesProcessed);
    State parseData(const nx::Buffer& buf, size_t* bytesProcessed);

private:
    State m_state;
    ModbusMessage* m_outputMessage;
    std::size_t m_messageDataLength;

};

}
