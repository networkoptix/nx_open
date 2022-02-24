// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "repeating_buffer_msg_body_source.h"

namespace nx::network::http {

RepeatingBufferMsgBodySource::RepeatingBufferMsgBodySource(
    const std::string& mimeType,
    nx::Buffer buffer)
    :
    m_mimeType(mimeType),
    m_buffer(std::move(buffer))
{
}

std::string RepeatingBufferMsgBodySource::mimeType() const
{
    return m_mimeType;
}

std::optional<uint64_t> RepeatingBufferMsgBodySource::contentLength() const
{
    return std::nullopt;
}

void RepeatingBufferMsgBodySource::readAsync(
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)
    > completionHandler)
{
    completionHandler(SystemError::noError, m_buffer);
}

} // namespace nx::network::http
