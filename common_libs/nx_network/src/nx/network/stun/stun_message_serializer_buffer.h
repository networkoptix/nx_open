#pragma once

#include <nx/network/buffer.h>

namespace nx {
namespace stun {

class NX_NETWORK_API MessageSerializerBuffer
{
public:
    MessageSerializerBuffer(nx::Buffer* buffer);

    void* WriteUint16(std::uint16_t value);
    void* WriteUint32(std::uint32_t value);
    void* WriteIPV6Address(const std::uint16_t* value);
    void* WriteByte(char byte);
    /**
     * @return NULL if not enough space in internal buffer.
     *     Otherwise, pointer to the beginning of data written.
     */
    void* WriteBytes(const char* bytes, std::size_t length);
    void* Poke(std::size_t size);
    std::size_t position() const;
    std::size_t size() const;
    // Use this function to set up the message length position
    std::uint16_t* WriteMessageLength();
    std::uint16_t* WriteMessageLength(std::uint16_t length);
    const nx::Buffer* buffer() const;

private:
    nx::Buffer* m_buffer;
    std::uint16_t* m_headerLength;
};

} // namespace stun
} // namespace nx
