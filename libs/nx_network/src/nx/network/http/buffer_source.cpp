// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "buffer_source.h"

namespace nx::network::http {

BufferSource::BufferSource(std::string mimeType, nx::Buffer msgBody):
    m_mimeType(std::move(mimeType)),
    m_msgBody(std::move(msgBody))
{
}

std::string BufferSource::mimeType() const
{
    return m_mimeType;
}

std::optional<uint64_t> BufferSource::contentLength() const
{
    return m_msgBody.size();
}

void BufferSource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)
    > completionHandler)
{
    nx::Buffer outMsgBody;
    outMsgBody.swap(m_msgBody);
    completionHandler(SystemError::noError, std::move(outMsgBody));
}

std::optional<std::tuple<SystemError::ErrorCode, nx::Buffer>> BufferSource::peek()
{
    return std::make_tuple(SystemError::noError, std::exchange(m_msgBody, {}));
}

void BufferSource::cancelRead()
{
    // No need to cancel anything with current readAsync implementation.
}

} // namespace nx::network::http
