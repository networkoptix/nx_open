/**********************************************************
* 8 may 2015
* a.kolesnikov
***********************************************************/

#include "base64_decoder_filter.h"


Base64DecoderFilter::Base64DecoderFilter()
{
}

Base64DecoderFilter::~Base64DecoderFilter()
{
}

bool Base64DecoderFilter::processData( const QnByteArrayConstRef& data )
{
    return m_nextFilter->processData( QByteArray::fromBase64( data.toByteArrayWithRawData() ) );
}

size_t Base64DecoderFilter::flush()
{
    return 0;
}
