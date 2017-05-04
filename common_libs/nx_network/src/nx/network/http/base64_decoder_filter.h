#pragma once 

#include <nx/utils/abstract_byte_stream_filter.h>

/**
 * Input: base64 string
 * Output: decoded string
 * @note Currently, all source buffers are decoded independently.
 */
class NX_NETWORK_API Base64DecoderFilter:
    public nx::utils::bsf::AbstractByteStreamFilter
{
public:
    Base64DecoderFilter();
    virtual ~Base64DecoderFilter();

    virtual bool processData( const QnByteArrayConstRef& data ) override;
    virtual size_t flush() override;
};
