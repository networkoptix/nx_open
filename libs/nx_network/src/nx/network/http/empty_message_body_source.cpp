// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "empty_message_body_source.h"

namespace nx::network::http {

EmptyMessageBodySource::EmptyMessageBodySource(
    std::string contentType,
    const std::optional<uint64_t>& contentLength)
    :
    m_contentType(std::move(contentType)),
    m_contentLength(contentLength)
{
}

std::string EmptyMessageBodySource::mimeType() const
{
    return m_contentType;
}

std::optional<uint64_t> EmptyMessageBodySource::contentLength() const
{
    return m_contentLength;
}

void EmptyMessageBodySource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)> completionHandler)
{
    post(
        [completionHandler = std::move(completionHandler)]()
        {
            completionHandler(SystemError::noError, nx::Buffer());
        });
}

} // namespace nx::network::http
