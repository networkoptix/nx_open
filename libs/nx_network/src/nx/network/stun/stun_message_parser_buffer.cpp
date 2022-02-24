// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stun_message_parser_buffer.h"

#include <QtCore/QtEndian>

namespace nx::network::stun {

MessageParserBuffer::MessageParserBuffer(const nx::ConstBufferRefType& buffer):
    m_buffer(buffer)
{
}

std::uint16_t MessageParserBuffer::NextUint16(bool* ok)
{
    std::uint16_t value = 0;
    if (!read(sizeof(std::uint16_t), &value))
    {
        *ok = false;
        return 0;
    }
    *ok = true;
    return qFromBigEndian(value);
}

std::uint32_t MessageParserBuffer::NextUint32(bool* ok)
{
    std::uint32_t value = 0;
    if (!read(sizeof(std::uint32_t), &value))
    {
        *ok = false;
        return 0;
    }
    *ok = true;
    return qFromBigEndian(value);
}

std::uint8_t MessageParserBuffer::NextByte(bool* ok)
{
    std::uint8_t value = 0;
    if (!read(sizeof(std::uint8_t), &value))
    {
        *ok = false;
        return 0;
    }
    *ok = true;
    return value;
}

void MessageParserBuffer::readNextBytesToBuffer(
    char* bytes,
    std::size_t sz,
    bool* ok)
{
    if (!read(sz, bytes))
    {
        *ok = false;
        return;
    }
    *ok = true;
}

std::size_t MessageParserBuffer::position() const
{
    return m_position;
}

bool MessageParserBuffer::eof() const
{
    return m_position == m_buffer.size();
}

bool MessageParserBuffer::read(std::size_t count, void* buffer)
{
    if (m_buffer.size() - m_position < count)
        return false;

    memcpy(reinterpret_cast<char*>(buffer), m_buffer.data() + m_position, count);
    m_position += count;
    return true;
}

} // namespace nx::network::stun
