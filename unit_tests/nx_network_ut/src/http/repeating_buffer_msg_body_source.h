/**********************************************************
* Aug 25, 2015
* a.kolesnikov
***********************************************************/

#ifndef REPEATING_BUFFER_MSG_BODY_SOURCE_H
#define REPEATING_BUFFER_MSG_BODY_SOURCE_H

#include <nx/network/aio/timer.h>
#include <nx/network/http/abstract_msg_body_source.h>


class RepeatingBufferMsgBodySource
:
    public nx_http::AbstractMsgBodySource
{
public:
    RepeatingBufferMsgBodySource(
        const nx_http::StringType& mimeType,
        nx::Buffer buffer);

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override;
    virtual void bindToAioThread(
        nx::network::aio::AbstractAioThread* aioThread) override;
    virtual void post(nx::utils::MoveOnlyFunc<void()> func) override;
    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> func) override;

    //!Implementation of nx_http::AbstractMsgBodySource::mimeType
    virtual nx_http::StringType mimeType() const override;
    //!Implementation of nx_http::AbstractMsgBodySource::contentLength
    virtual boost::optional<uint64_t> contentLength() const override;
    /** Provides \a buffer forever */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx_http::BufferType)
        > completionHandler) override;

private:
    const nx_http::StringType m_mimeType;
    nx::Buffer m_buffer;
    nx::network::aio::Timer m_timer;
};

#endif  //REPEATING_BUFFER_MSG_BODY_SOURCE_H
