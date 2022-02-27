// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>
#include <optional>

#include "abstract_msg_body_source.h"
#include "server/http_server_connection.h"

namespace nx::network::http {

/**
 * Wraps HTTP response with a message body.
 */
class NX_NETWORK_API HttpClientMessageBodySource:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    HttpClientMessageBodySource(
        const Response& response,
        std::unique_ptr<AsyncMessagePipeline> messagePipeline);

    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual std::string mimeType() const override;

    virtual std::optional<uint64_t> contentLength() const override;

    virtual void readAsync(CompletionHandler completionHandler) override;

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::string m_mimeType;
    std::optional<uint64_t> m_contentLength;
    std::unique_ptr<AsyncMessagePipeline> m_messagePipeline;
    bool m_eof = false;
    std::deque<nx::Buffer> m_messageBodyChunks;
    std::optional<CompletionHandler> m_handler;

    void onSomeMessageBodyAvailable(nx::Buffer buffer);
    void onMessageEnd();
    void onConnectionClosed(SystemError::ErrorCode reason);
};

} // namespace nx::network::http
