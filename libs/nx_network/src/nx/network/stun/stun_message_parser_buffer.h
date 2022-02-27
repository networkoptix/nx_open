// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <nx/utils/buffer.h>

namespace nx::network::stun {

// This message parser buffer add a simple workaround with the original partial data buffer.
// It will consume all the data in the user buffer, but provide a more consistent interface
// for user. So if the parser stuck at one byte data, the MessageParserBuffer will temporary
// store that one byte and make user buffer drained. However without losing any single bytes
class NX_NETWORK_API MessageParserBuffer
{
public:
    MessageParserBuffer(const nx::ConstBufferRefType& buffer);

    std::uint16_t NextUint16(bool* ok);
    std::uint32_t NextUint32(bool* ok);
    std::uint8_t NextByte(bool* ok);
    void readNextBytesToBuffer(char* buffer, std::size_t count, bool* ok);
    std::size_t position() const;
    bool eof() const;

private:
    bool read(std::size_t byteSize, void* buffer);

private:
    nx::ConstBufferRefType m_buffer;
    std::size_t m_position = 0;
};

} // namespace nx::network::stun
