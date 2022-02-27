// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

namespace nx {
namespace network {
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

    std::size_t remainingCapacity() const;

private:
    nx::Buffer* m_buffer;
    std::uint16_t* m_headerLength = nullptr;
};

} // namespace stun
} // namespace network
} // namespace nx
