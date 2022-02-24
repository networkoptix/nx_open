// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_byte_stream_filter.h"

namespace nx {
namespace utils {
namespace bstream {

/**
 * Decodes stream of following blocks:
 * - 4 bytes. size in network byte order.
 * - n bytes of data.
 * Next filter receives one block per SizedDataDecodingFilter::processData call.
 */
class NX_UTILS_API SizedDataDecodingFilter:
    public AbstractByteStreamFilter
{
public:
    virtual bool processData(const ConstBufferRefType& data) override;
};

} // namespace bstream
} // namespace utils
} // namespace nx
