// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_client_message_body_source.h"

namespace nx::network::http {

HttpClientMessageBodySource::HttpClientMessageBodySource(
    const Response& response,
    std::unique_ptr<AsyncMessagePipeline> messagePipeline)
    :
    m_messagePipeline(std::move(messagePipeline))
{
    bindToAioThread(m_messagePipeline->getAioThread());

    m_mimeType = getHeaderValue(response.headers, "Content-Type");
    if (auto it = response.headers.find("Content-Length"); it != response.headers.end())
        m_contentLength = nx::utils::stoull(it->second);

    m_messagePipeline->setOnSomeMessageBodyAvailable(
        [this](auto&&... args) { onSomeMessageBodyAvailable(std::forward<decltype(args)>(args)...); });
    m_messagePipeline->setOnMessageEnd(
        [this](auto&&... args) { onMessageEnd(std::forward<decltype(args)>(args)...); });
    m_messagePipeline->registerCloseHandler(
        [this](auto reason, auto /*connectionDestroyed*/) { onConnectionClosed(reason); });
}

void HttpClientMessageBodySource::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_messagePipeline)
        m_messagePipeline->bindToAioThread(aioThread);
}

std::string HttpClientMessageBodySource::mimeType() const
{
    return m_mimeType;
}

std::optional<uint64_t> HttpClientMessageBodySource::contentLength() const
{
    return m_contentLength;
}

void HttpClientMessageBodySource::readAsync(
    CompletionHandler completionHandler)
{
    post(
        [this, completionHandler = std::move(completionHandler)]() mutable
        {
            NX_ASSERT(!m_handler);

            if (!m_messageBodyChunks.empty())
            {
                auto buf = std::move(m_messageBodyChunks.front());
                m_messageBodyChunks.pop_front();
                return completionHandler(SystemError::noError, std::move(buf));
            }

            if (m_eof)
                return completionHandler(SystemError::noError, nx::Buffer());

            m_handler = std::move(completionHandler);
        });
}

void HttpClientMessageBodySource::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_messagePipeline.reset();
}

void HttpClientMessageBodySource::onSomeMessageBodyAvailable(nx::Buffer buffer)
{
    if (m_handler)
    {
        NX_ASSERT(m_messageBodyChunks.empty());
        auto handler = std::move(*m_handler);
        m_handler = std::nullopt;
        return handler(SystemError::noError, std::move(buffer));
    }

    if (m_messageBodyChunks.empty() || buffer.empty())
        m_messageBodyChunks.push_back(std::move(buffer));
    else
        m_messageBodyChunks.back() += std::move(buffer);
}

void HttpClientMessageBodySource::onMessageEnd()
{
    NX_VERBOSE(this, "Message end");

    m_eof = true;
    onSomeMessageBodyAvailable(nx::Buffer());
}

void HttpClientMessageBodySource::onConnectionClosed(
    SystemError::ErrorCode reason)
{
    NX_VERBOSE(this, "Connection closed with %1", SystemError::toString(reason));

    m_eof = true;
    onSomeMessageBodyAvailable(nx::Buffer());
}

} // namespace nx::network::http
