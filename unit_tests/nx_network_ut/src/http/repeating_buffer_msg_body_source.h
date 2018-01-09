#pragma once

#include <nx/network/aio/timer.h>
#include <nx/network/http/abstract_msg_body_source.h>

class RepeatingBufferMsgBodySource:
    public nx::network::http::AbstractMsgBodySource
{
public:
    RepeatingBufferMsgBodySource(
        const nx::network::http::StringType& mimeType,
        nx::Buffer buffer);
    ~RepeatingBufferMsgBodySource();

    virtual void stopWhileInAioThread() override;

    virtual nx::network::http::StringType mimeType() const override;
    virtual boost::optional<uint64_t> contentLength() const override;
    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::network::http::BufferType)
        > completionHandler) override;

private:
    const nx::network::http::StringType m_mimeType;
    nx::Buffer m_buffer;
};
