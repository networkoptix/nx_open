#pragma once

#include <nx/utils/byte_stream/abstract_byte_stream_filter.h>

/**
 * Input: base64 string
 * Output: decoded string
 * NOTE: Currently, all source buffers are decoded independently.
 */
class NX_NETWORK_API Base64DecoderFilter:
    public nx::utils::bstream::AbstractByteStreamFilter
{
public:
    virtual bool processData(const QnByteArrayConstRef& data) override;
    virtual size_t flush() override;
};
