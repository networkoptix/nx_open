#include "repeating_buffer_msg_body_source.h"

RepeatingBufferMsgBodySource::RepeatingBufferMsgBodySource(
    const nx_http::StringType& mimeType,
    nx::Buffer buffer)
    :
    m_mimeType(mimeType),
    m_buffer(std::move(buffer))
{
}

RepeatingBufferMsgBodySource::~RepeatingBufferMsgBodySource()
{
    stopWhileInAioThread();
}

void RepeatingBufferMsgBodySource::stopWhileInAioThread()
{
}

nx_http::StringType RepeatingBufferMsgBodySource::mimeType() const
{
    return m_mimeType;
}

boost::optional<uint64_t> RepeatingBufferMsgBodySource::contentLength() const
{
    return boost::none;
}

void RepeatingBufferMsgBodySource::readAsync(
    nx::utils::MoveOnlyFunc<
    void(SystemError::ErrorCode, nx_http::BufferType)
    > completionHandler)
{
    completionHandler(SystemError::noError, m_buffer);
}
