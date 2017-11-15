#pragma once

#include <cstdint>
#include <memory>

#include <boost/optional.hpp>

#include <nx/utils/std/cpp14.h>

#include "abstract_msg_body_source.h"

namespace nx_http {

template<typename AsyncChannel>
class AsyncChannelMessageBodySource:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    AsyncChannelMessageBodySource(
        StringType mimeType,
        std::unique_ptr<AsyncChannel> channel)
        :
        m_mimeType(std::move(mimeType)),
        m_channel(std::move(channel))
    {
        bindToAioThread(m_channel->getAioThread());
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_channel->bindToAioThread(aioThread);
    }

    virtual StringType mimeType() const override
    {
        return m_mimeType;
    }

    virtual boost::optional<uint64_t> contentLength() const override
    {
        return m_messageBodyLimit;
    }

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)> completionHandler) override
    {
        const std::size_t kReadBufferSize = 16 * 1024;

        using namespace std::placeholders;

        if (messageBodyTransferLimitHasBeenReached())
            return reportEndOfMessageBody(std::move(completionHandler));

        m_completionHandler.swap(completionHandler);

        m_readBuffer.reserve(kReadBufferSize);
        m_channel->readSomeAsync(
            &m_readBuffer,
            std::bind(&AsyncChannelMessageBodySource::onSomeBytesRead, this, _1, _2));
    }

    /**
     * End of message body will be reported after sending messageBodyLimit bytes.
     */
    void setMessageBodyLimit(std::uint64_t messageBodyLimit)
    {
        m_messageBodyLimit = messageBodyLimit;
    }

private:
    StringType m_mimeType;
    std::unique_ptr<AsyncChannel> m_channel;
    nx_http::BufferType m_readBuffer;
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, BufferType)> m_completionHandler;
    boost::optional<std::uint64_t> m_messageBodyLimit;
    std::uint64_t m_totalBytesSent = 0;

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_channel.reset();
    }

    bool messageBodyTransferLimitHasBeenReached() const
    {
        return m_messageBodyLimit && (m_totalBytesSent >= *m_messageBodyLimit);
    }

    void reportEndOfMessageBody(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, BufferType)> completionHandler)
    {
        return post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(SystemError::noError, nx::Buffer());
            });
    }

    void onSomeBytesRead(
        SystemError::ErrorCode systemErrorCode,
        std::size_t /*bytesRead*/)
    {
        nx_http::BufferType readBuffer;
        m_readBuffer.swap(readBuffer);

        if (m_messageBodyLimit && (m_totalBytesSent + readBuffer.size() > *m_messageBodyLimit))
            readBuffer.remove(*m_messageBodyLimit - m_totalBytesSent, readBuffer.size());

        m_totalBytesSent += readBuffer.size();

        nx::utils::swapAndCall(
            m_completionHandler,
            systemErrorCode,
            std::move(readBuffer));
    }
};

template<typename AsyncChannel>
std::unique_ptr<AsyncChannelMessageBodySource<AsyncChannel>>
    makeAsyncChannelMessageBodySource(
        nx_http::StringType mimeType,
        std::unique_ptr<AsyncChannel> asyncChannel)
{
    return std::make_unique<AsyncChannelMessageBodySource<AsyncChannel>>(
        std::move(mimeType),
        std::move(asyncChannel));
}

} // namespace nx_http
