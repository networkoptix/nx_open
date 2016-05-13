/**********************************************************
* Aug 25, 2015
* a.kolesnikov
***********************************************************/

#include "repeating_buffer_msg_body_source.h"


RepeatingBufferMsgBodySource::RepeatingBufferMsgBodySource(
    const nx_http::StringType& mimeType,
    nx::Buffer buffer)
:
    m_mimeType(mimeType),
    m_buffer(std::move(buffer))
{
}

void RepeatingBufferMsgBodySource::pleaseStop(
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_timer.pleaseStop(std::move(handler));
}

nx::network::aio::AbstractAioThread* RepeatingBufferMsgBodySource::getAioThread() const
{
    return m_timer.getAioThread();
}

void RepeatingBufferMsgBodySource::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_timer.bindToAioThread(aioThread);
}

void RepeatingBufferMsgBodySource::post(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.post(std::move(func));
}

void RepeatingBufferMsgBodySource::dispatch(nx::utils::MoveOnlyFunc<void()> func)
{
    m_timer.dispatch(std::move(func));
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
