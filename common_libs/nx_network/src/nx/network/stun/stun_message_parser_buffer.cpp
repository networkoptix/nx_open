#include "stun_message_parser_buffer.h"

#include <QtEndian>

namespace nx {
namespace stun {

MessageParserBuffer::MessageParserBuffer(std::deque<char>* temp_buffer, const nx::Buffer& buffer):
    m_tempBuffer(temp_buffer),
    m_buffer(buffer),
    m_position(0)
{
}

MessageParserBuffer::MessageParserBuffer(const nx::Buffer& buffer):
    m_tempBuffer(NULL),
    m_buffer(buffer),
    m_position(0)
{
}

std::uint16_t MessageParserBuffer::NextUint16(bool* ok)
{
    std::uint16_t value = 0;
    if (!ensure(sizeof(quint16), &value))
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
    if (!ensure(sizeof(quint32), &value))
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
    if (!ensure(sizeof(quint8), &value))
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
    if (!ensure(sz, bytes))
    {
        *ok = false;
        return;
    }
    *ok = true;
}

void MessageParserBuffer::clear()
{
    if (m_tempBuffer != NULL)
        m_tempBuffer->clear();
}

std::size_t MessageParserBuffer::position() const
{
    return m_position;
}

// ensure will check the temporary buffer and also the buffer in nx::Buffer.
// Additionally, if ensure cannot ensure the buffer size, it will still drain
// the original buffer , so the user buffer will always be consumed over .
bool MessageParserBuffer::ensure(std::size_t count, void* buffer)
{
    // Get the available bytes that we can read from the stream
    const std::size_t availableBytes =
        (m_tempBuffer == NULL ? 0 : m_tempBuffer->size()) +
        m_buffer.size() - m_position;
    if (availableBytes < count)
    {
        // Drain the user buffer here
        if (m_tempBuffer != NULL)
        {
            for (std::size_t i = m_position; i < (std::size_t)m_buffer.size(); ++i)
                m_tempBuffer->push_back(m_buffer.at((int)i));
            // Modify the position pointer here
            m_position = m_buffer.size();
        }
        return false;
    }
    else
    {
        std::size_t pos = 0;
        // 1. Drain the buffer from the temporary buffer here
        if (m_tempBuffer != NULL)
        {
            const std::size_t temp_buffer_size = m_tempBuffer->size();
            for (std::size_t i = 0; i < temp_buffer_size && count != pos; ++i, ++pos)
            {
                *(reinterpret_cast<char*>(buffer) + pos) = m_tempBuffer->front();
                m_tempBuffer->pop_front();
            }
            if (count == pos)
            {
                return true;
            }
        }
        // 2. Finish the buffer feeding
        memcpy(
            reinterpret_cast<char*>(buffer) + pos,
            m_buffer.constData() + m_position,
            count - pos);
        m_position += count - pos;
        return true;
    }
}

} // namespace stun
} // namespace nx
