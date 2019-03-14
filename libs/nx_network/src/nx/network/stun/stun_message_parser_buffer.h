#pragma once

#include <deque>

#include <nx/network/buffer.h>

namespace nx {
namespace network {
namespace stun {

// This message parser buffer add a simple workaround with the original partial data buffer.
// It will consume all the data in the user buffer, but provide a more consistent interface
// for user. So if the parser stuck at one byte data, the MessageParserBuffer will temporary
// store that one byte and make user buffer drained. However without losing any single bytes
class NX_NETWORK_API MessageParserBuffer
{
public:
    MessageParserBuffer(std::deque<char>* temp_buffer, const nx::Buffer& buffer);
    MessageParserBuffer(const nx::Buffer& buffer);

    std::uint16_t NextUint16(bool* ok);
    std::uint32_t NextUint32(bool* ok);
    std::uint8_t NextByte(bool* ok);
    void readNextBytesToBuffer(char* buffer, std::size_t count, bool* ok);
    // Do not modify the temp_buffer_size_ here
    void clear();
    std::size_t position() const;

private:
    std::deque<char>* m_tempBuffer;
    const nx::Buffer& m_buffer;
    std::size_t m_position;

    bool ensure(std::size_t byteSize, void* buffer);

    Q_DISABLE_COPY(MessageParserBuffer)
};

} // namespace stun
} // namespace network
} // namespace nx
