#include "stun_message_serializer_buffer.h"

#include <QtEndian>

#include <nx/utils/log/assert.h>

namespace nx {
namespace network {
namespace stun {

MessageSerializerBuffer::MessageSerializerBuffer(nx::Buffer* buffer):
    m_buffer(buffer),
    m_headerLength(NULL)
{
}

void* MessageSerializerBuffer::Poke(std::size_t size)
{
    if (size > static_cast< size_t >(m_buffer->capacity() - m_buffer->size()))
        return NULL;

    const auto oldSize = m_buffer->size();
    m_buffer->resize(m_buffer->size() + static_cast< int >(size));
    return m_buffer->data() + oldSize;
}

// Write the uint16 value into the buffer with bytes order swap
void* MessageSerializerBuffer::WriteUint16(std::uint16_t value)
{
    void* ret = Poke(sizeof(std::uint16_t));
    if (ret == NULL)
    {
        return NULL;
    }
    else
    {
        qToBigEndian(value, reinterpret_cast<uchar*>(ret));
        return ret;
    }
}

void* MessageSerializerBuffer::WriteIPV6Address(const std::uint16_t* value)
{
    void* buffer = Poke(16);
    if (buffer == NULL) return NULL;
    for (std::size_t i = 0; i < 8; ++i)
    {
        qToBigEndian(value[i], reinterpret_cast<uchar*>(reinterpret_cast<std::uint16_t*>(buffer) + i));
    }
    return buffer;
}

void* MessageSerializerBuffer::WriteUint32(std::uint32_t value)
{
    void* ret = Poke(sizeof(std::uint32_t));
    if (ret == NULL)
    {
        return NULL;
    }
    else
    {
        qToBigEndian(value, reinterpret_cast<uchar*>(ret));
        return ret;
    }
}

// Write the byte into the specific position
void* MessageSerializerBuffer::WriteByte(char byte)
{
    return WriteBytes(&byte, 1U);
}

void* MessageSerializerBuffer::WriteBytes(const char* bytes, std::size_t length)
{
    void* ret = Poke(length);
    if (ret == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(ret, bytes, length);
        return ret;
    }
}

std::size_t MessageSerializerBuffer::position() const
{
    return static_cast<std::size_t>(m_buffer->size());
}

std::size_t MessageSerializerBuffer::size() const
{
    return m_buffer->size();
}

// Use this function to set up the message length position
std::uint16_t* MessageSerializerBuffer::WriteMessageLength()
{
    NX_ASSERT(m_headerLength == NULL);
    void* ret = Poke(2);
    if (ret == NULL) return NULL;
    m_headerLength = reinterpret_cast<std::uint16_t*>(ret);
    return m_headerLength;
}

std::uint16_t* MessageSerializerBuffer::WriteMessageLength(std::uint16_t length)
{
    NX_ASSERT(m_headerLength != NULL);
    qToBigEndian(length, reinterpret_cast<uchar*>(m_headerLength));
    return m_headerLength;
}

const nx::Buffer* MessageSerializerBuffer::buffer() const
{
    return m_buffer;
}

} // namespace stun
} // namespace network
} // namespace nx
