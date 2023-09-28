// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

/**
 * Saves all incoming data to internal buffer.
 */
class NX_UTILS_API BufferOutputStream:
    public AbstractByteStreamFilter
{
public:
    BufferOutputStream();

    virtual bool processData(const ConstBufferRefType& data) override;

    nx::Buffer buffer() const;

private:
    nx::Buffer m_buffer;
};

} // namespace bstream
} // namespace utils
} // namespace nx
