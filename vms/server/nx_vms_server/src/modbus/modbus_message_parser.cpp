#include "modbus_message_parser.h"

namespace nx
{
namespace modbus
{

ModbusMessageParser::ModbusMessageParser() :
    m_state(ModbusMessageParser::State::initial),
    m_outputMessage(nullptr),
    m_messageDataLength(0)
{
}

void ModbusMessageParser::setMessage(ModbusMessage* const msg)
{
    m_outputMessage = msg;
}

nx::network::server::ParserState ModbusMessageParser::parse(
    const nx::Buffer& buf, size_t* bytesProcessed)
{
    m_state = ModbusMessageParser::State::parsingHeader;

    if (parseHeader(buf, bytesProcessed) != ModbusMessageParser::State::parsingData)
        return state();

    parseData(buf, bytesProcessed);
    
    return state();
}

void ModbusMessageParser::reset()
{
    m_state = ModbusMessageParser::State::initial;
}

nx::network::server::ParserState ModbusMessageParser::state() const
{
    switch (m_state)
    {
        case ModbusMessageParser::State::initial:
            return nx::network::server::ParserState::init;
        case ModbusMessageParser::State::done:
            return nx::network::server::ParserState::done;
        case ModbusMessageParser::State::failed:
            return nx::network::server::ParserState::failed;
        default:
            return nx::network::server::ParserState::readingMessage;
    }
}

ModbusMessageParser::State ModbusMessageParser::parseHeader(
    const nx::Buffer& buffer, std::size_t* /*bytesProcessed*/)
{
    NX_ASSERT(m_state == ModbusMessageParser::State::parsingHeader);

    std::size_t bufferSize = (std::size_t)buffer.size();

    if (bufferSize < ModbusMessage::kHeaderSize)
        return ModbusMessageParser::State::parsingHeader;

    m_outputMessage->header = ModbusMBAPHeader::decode(buffer);
    m_outputMessage->functionCode = buffer[ModbusMessage::kHeaderSize - 1];

    m_messageDataLength = m_outputMessage->header.length 
        - sizeof(decltype(ModbusMBAPHeader::unitId))
        - sizeof(decltype(ModbusMessage::functionCode));

    m_state = ModbusMessageParser::State::parsingData;

    return m_state;
}

ModbusMessageParser::State ModbusMessageParser::parseData(const nx::Buffer& buffer, size_t* bytesProcessed)
{
    NX_ASSERT(m_state == ModbusMessageParser::State::parsingData);

    const std::size_t kFullMessageLength = ModbusMessage::kHeaderSize + m_messageDataLength;
    const std::size_t kBufferSize = (std::size_t)buffer.size();

    if (kBufferSize < kFullMessageLength)
        return ModbusMessageParser::State::parsingData;

    m_outputMessage->data = buffer.mid(ModbusMessage::kHeaderSize);

    *bytesProcessed = kFullMessageLength;

    m_state = ModbusMessageParser::State::done;

    return m_state;
}

} //< Closing namespace modbus

} //< Closing namespace nx
