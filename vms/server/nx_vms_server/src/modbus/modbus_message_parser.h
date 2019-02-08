#pragma once

#include "modbus.h"

#include <nx/network/connection_server/base_protocol_message_types.h>

namespace nx {
namespace modbus {

class ModbusMessageParser:
    public nx::network::server::AbstractMessageParser<ModbusMessage>
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

    virtual void setMessage(ModbusMessage* const msg) override;

    virtual nx::network::server::ParserState parse(
        const nx::Buffer& buf, size_t* bytesProcessed) override;

    virtual void reset() override;

    nx::network::server::ParserState state() const;

private:
    State parseHeader(const nx::Buffer& buf, size_t* bytesProcessed);
    State parseData(const nx::Buffer& buf, size_t* bytesProcessed);

private:
    State m_state;
    ModbusMessage* m_outputMessage;
    std::size_t m_messageDataLength;

};

} // namespace modbus
} // namespace nx

