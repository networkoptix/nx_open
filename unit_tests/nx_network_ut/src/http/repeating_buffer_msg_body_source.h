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
    ~RepeatingBufferMsgBodySource();

    virtual void stopWhileInAioThread() override;

    //!Implementation of nx_http::AbstractMsgBodySource::mimeType
    virtual nx_http::StringType mimeType() const override;
    //!Implementation of nx_http::AbstractMsgBodySource::contentLength
    virtual boost::optional<uint64_t> contentLength() const override;
    /** Provides buffer forever */
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx_http::BufferType)
        > completionHandler) override;

private:
    const nx_http::StringType m_mimeType;
    nx::Buffer m_buffer;
};

#endif  //REPEATING_BUFFER_MSG_BODY_SOURCE_H
