// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

namespace nx::network::http {

/**
 * Input: base64 string
 * Output: decoded string
 * NOTE: Currently, all source buffers are decoded independently.
 */
class NX_NETWORK_API Base64DecoderFilter:
    public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    virtual bool processData(const nx::ConstBufferRefType& data) override;
    virtual size_t flush() override;
};

} // namespace nx::network::http
