#include "base64_decoder_filter.h"

bool Base64DecoderFilter::processData(const QnByteArrayConstRef& data)
{
    return m_nextFilter->processData(QByteArray::fromBase64(data.toByteArrayWithRawData()));
}

size_t Base64DecoderFilter::flush()
{
    return 0;
}
