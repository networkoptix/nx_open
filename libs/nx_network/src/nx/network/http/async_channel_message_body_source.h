// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>
#include <memory>
#include <optional>

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include "abstract_msg_body_source.h"

namespace nx {
namespace network {
namespace http {

template<typename AsyncChannel>
class AsyncChannelMessageBodySource:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    AsyncChannelMessageBodySource(
        std::string mimeType,
        std::unique_ptr<AsyncChannel> channel)
        :
        m_mimeType(std::move(mimeType)),
        m_channel(std::move(channel))
    {
        bindToAioThread(m_channel->getAioThread());

        NX_VERBOSE(this, "Created message body source. MIME type %1", m_mimeType);
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_channel->bindToAioThread(aioThread);
    }

    virtual std::string mimeType() const override
    {
        return m_mimeType;
    }

    virtual std::optional<uint64_t> contentLength() const override
    {
        return m_messageBodyLimit;
    }

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx::Buffer)> completionHandler) override
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

    /**
     * Extracts source channel.
     */
    std::unique_ptr<AsyncChannel> takeChannel()
    {
        return std::exchange(m_channel, nullptr);
    }

private:
    std::string m_mimeType;
    std::unique_ptr<AsyncChannel> m_channel;
    nx::Buffer m_readBuffer;
    nx::utils::MoveOnlyFunc<
        void(SystemError::ErrorCode, nx::Buffer)> m_completionHandler;
    std::optional<std::uint64_t> m_messageBodyLimit;
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
            void(SystemError::ErrorCode, nx::Buffer)> completionHandler)
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
        nx::Buffer readBuffer;
        m_readBuffer.swap(readBuffer);

        // TODO: #akolesnikov Think over the following condition. Probably, an assert should be added.
        if (m_messageBodyLimit && (m_totalBytesSent + readBuffer.size() > *m_messageBodyLimit))
            readBuffer.erase(*m_messageBodyLimit - m_totalBytesSent);

        m_totalBytesSent += readBuffer.size();

        NX_VERBOSE(this, "Read another %1 bytes (errorCode %2). Total bytes read %3. "
            "Message body limit %4",
            readBuffer.size(), SystemError::toString(systemErrorCode), m_totalBytesSent,
            m_messageBodyLimit);

        nx::utils::swapAndCall(
            m_completionHandler,
            systemErrorCode,
            std::move(readBuffer));
    }
};

template<typename AsyncChannel>
std::unique_ptr<AsyncChannelMessageBodySource<AsyncChannel>>
    makeAsyncChannelMessageBodySource(
        std::string mimeType,
        std::unique_ptr<AsyncChannel> asyncChannel)
{
    return std::make_unique<AsyncChannelMessageBodySource<AsyncChannel>>(
        std::move(mimeType),
        std::move(asyncChannel));
}

} // namespace nx
} // namespace network
} // namespace http
