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

nx_api::SerializerState::Type ModbusMessageSerializer::serialize(nx::Buffer* const buffer, size_t* const bytesWritten)
{
    if (serializeHeader(buffer) == nx_api::SerializerState::needMoreBufferSpace)
        return nx_api::SerializerState::needMoreBufferSpace;
    if (serializeData(buffer) == nx_api::SerializerState::needMoreBufferSpace)
        return nx_api::SerializerState::needMoreBufferSpace;

    return nx_api::SerializerState::done;
}

nx::Buffer ModbusMessageSerializer::serialized(const ModbusMessage& message)
{
    return message.encode();
}

nx_api::SerializerState::Type ModbusMessageSerializer::serializeHeader(nx::Buffer* const buffer)
{
    auto bufferFreeSpace = buffer->capacity() - buffer->size();
    if (bufferFreeSpace < ModbusMessage::kHeaderSize)
        return nx_api::SerializerState::needMoreBufferSpace;

    buffer->append(ModbusMBAPHeader::encode(m_message->header));
    buffer->append(m_message->functionCode);

    return nx_api::SerializerState::done;
}

nx_api::SerializerState::Type ModbusMessageSerializer::serializeData(nx::Buffer* const buffer)
{
    auto bufferFreeSpace = buffer->capacity() - buffer->size();
    if (bufferFreeSpace < m_message->data.size())
        return nx_api::SerializerState::needMoreBufferSpace;

    buffer->append(m_message->data);

    return nx_api::SerializerState::done;
}

} //< Closing namespace modbus

} //< Closing namespace nx
