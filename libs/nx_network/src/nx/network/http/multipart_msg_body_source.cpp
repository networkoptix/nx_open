#include "multipart_msg_body_source.h"

#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx::network::http {

MultipartMessageBodySource::MultipartMessageBodySource(StringType boundary):
    base_type("multipart/x-mixed-replace;boundary=" + boundary.toStdString()),
    m_multipartBodySerializer(
        std::move(boundary),
        nx::utils::bstream::makeCustomOutputStream(
            [this](auto&&... args) { return onSomeDataAvailable(std::move(args)...); }))
{
}

MultipartBodySerializer* MultipartMessageBodySource::serializer()
{
    return &m_multipartBodySerializer;
}

void MultipartMessageBodySource::onSomeDataAvailable(
    const QnByteArrayConstRef& data)
{
    NX_ASSERT(!data.isEmpty());

    writeBodyData(data);

    if (m_multipartBodySerializer.eof())
        writeBodyData(QnByteArrayConstRef());
}

} // namespace nx::network::http
