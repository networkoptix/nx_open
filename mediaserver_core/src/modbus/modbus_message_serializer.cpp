#include "modbus_message_serializer.h"

namespace nx
{

namespace modbus
{

ModbusMessageSerializer::ModbusMessageSerializer():
    m_message(nullptr),
    m_initialized(false)
{
}

void ModbusMessageSerializer::setMessage(const ModbusMessage* message)
{
    m_message = message;
}

nx::network::server::SerializerState ModbusMessageSerializer::serialize(nx::Buffer* const buffer, size_t* const bytesWritten)
{
    if (serializeHeader(buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
        return nx::network::server::SerializerState::needMoreBufferSpace;
    if (serializeData(buffer) == nx::network::server::SerializerState::needMoreBufferSpace)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    return nx::network::server::SerializerState::done;
}

nx::Buffer ModbusMessageSerializer::serialized(const ModbusMessage& message)
{
    return message.encode();
}

nx::network::server::SerializerState ModbusMessageSerializer::serializeHeader(nx::Buffer* const buffer)
{
    auto bufferFreeSpace = buffer->capacity() - buffer->size();
    if (bufferFreeSpace < ModbusMessage::kHeaderSize)
        return nx::network::server::SerializerState::needMoreBufferSpace;

    buffer->append(ModbusMBAPHeader::encode(m_message->header));
    buffer->append(m_message->functionCode);

    return nx::network::server::SerializerState::done;
}

nx::network::server::SerializerState ModbusMessageSerializer::serializeData(nx::Buffer* const buffer)
{
    auto bufferFreeSpace = buffer->capacity() - buffer->size();
    if (bufferFreeSpace < m_message->data.size())
        return nx::network::server::SerializerState::needMoreBufferSpace;

    buffer->append(m_message->data);

    return nx::network::server::SerializerState::done;
}

} //< Closing namespace modbus

} //< Closing namespace nx
