// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buffer_output_stream.h"

namespace nx {
namespace utils {
namespace bstream {

BufferOutputStream::BufferOutputStream():
    AbstractByteStreamFilter(std::shared_ptr<AbstractByteStreamFilter>())
{
}

bool BufferOutputStream::processData(const ConstBufferRefType& data)
{
    m_buffer += data;
    return true;
}

nx::Buffer BufferOutputStream::buffer() const
{
    return m_buffer;
}

} // namespace bstream
} // namespace utils
} // namespace nx
