/**********************************************************
* 20 may 2015
* a.kolesnikov
***********************************************************/

#include "buffer_source.h"


namespace nx_http
{
    BufferSource::BufferSource(StringType mimeType, BufferType msgBody)
    :
        m_mimeType(std::move(mimeType)),
        m_msgBody(std::move(msgBody))
    {
    }

    void BufferSource::pleaseStop(
        nx::utils::MoveOnlyFunc<void()> completionHandler)
    {
        m_aioBinder.pleaseStop(std::move(completionHandler));
    }

    nx::network::aio::AbstractAioThread* BufferSource::getAioThread() const
    {
        return m_aioBinder.getAioThread();
    }

    void BufferSource::bindToAioThread(
        nx::network::aio::AbstractAioThread* aioThread)
    {
        m_aioBinder.bindToAioThread(aioThread);
    }

    void BufferSource::post(nx::utils::MoveOnlyFunc<void()> func)
    {
        m_aioBinder.post(std::move(func));
    }

    void BufferSource::dispatch(nx::utils::MoveOnlyFunc<void()> func)
    {
        m_aioBinder.dispatch(std::move(func));
    }

    StringType BufferSource::mimeType() const
    {
        return m_mimeType;
    }

    boost::optional<uint64_t> BufferSource::contentLength() const
    {
        return boost::optional<uint64_t>(m_msgBody.size());
    }

    void BufferSource::readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)
        > completionHandler)
    {
        auto outMsgBody = std::move(m_msgBody);
        m_msgBody = BufferType();   //moving to valid state
        completionHandler(SystemError::noError, std::move(outMsgBody));
    }
}
