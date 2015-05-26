/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#ifndef BASE64_DECODER_FILTER_H
#define BASE64_DECODER_FILTER_H

#include <utils/media/abstract_byte_stream_filter.h>


/*!
    Input: base64 string
    Output: decoded string
    \note Currently, all source buffers are decoded independently. //TODO #ak make stream decoder of this
*/
class Base64DecoderFilter
:
    public AbstractByteStreamFilter
{
public:
    Base64DecoderFilter();
    virtual ~Base64DecoderFilter();

    //!Implementation of AbstractByteStreamFilter::processData
    virtual void processData( const QnByteArrayConstRef& data ) override;
    //!Implementation of AbstractByteStreamFilter::flush
    virtual size_t flush() override;
};

#endif  //BASE64_DECODER_FILTER_H
