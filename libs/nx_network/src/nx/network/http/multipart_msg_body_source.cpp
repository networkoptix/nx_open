// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "multipart_msg_body_source.h"

#include <nx/utils/byte_stream/custom_output_stream.h>

namespace nx::network::http {

MultipartMessageBodySource::MultipartMessageBodySource(std::string boundary):
    base_type("multipart/x-mixed-replace;boundary=" + boundary),
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
    const ConstBufferRefType& data)
{
    NX_ASSERT(!data.empty());

    writeBodyData(data);

    if (m_multipartBodySerializer.eof())
        writeBodyData(ConstBufferRefType());
}

} // namespace nx::network::http
