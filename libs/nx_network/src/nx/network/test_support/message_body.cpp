// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "message_body.h"

namespace nx::network::http::test {

CustomLengthBody::CustomLengthBody(
    std::string mimeType, nx::Buffer msgBody, std::optional<std::size_t> len)
    :
    base_type(std::move(mimeType), std::move(msgBody)),
    m_len(len)
{
}

std::optional<uint64_t> CustomLengthBody::contentLength() const
{
    if (m_len)
        return m_len;
    else
        return base_type::contentLength();
}

} // namespace nx::network::http::test
