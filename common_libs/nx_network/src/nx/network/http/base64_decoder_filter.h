#pragma once 

#include <utils/media/abstract_byte_stream_filter.h>

/**
 * Input: base64 string
 * Output: decoded string
 * @note Currently, all source buffers are decoded independently.
 */
class NX_NETWORK_API Base64DecoderFilter:
    public AbstractByteStreamFilter
{
public:
    Base64DecoderFilter();
    virtual ~Base64DecoderFilter();

    //!Implementation of AbstractByteStreamFilter::processData
    virtual bool processData( const QnByteArrayConstRef& data ) override;
    //!Implementation of AbstractByteStreamFilter::flush
    virtual size_t flush() override;
};
