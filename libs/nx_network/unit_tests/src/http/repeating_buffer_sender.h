#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

/**
 * Sends specified buffer forever.
 */
class RepeatingBufferSender:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    RepeatingBufferSender(const nx::network::http::StringType& mimeType, nx::Buffer buffer);

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const nx::network::http::StringType m_mimeType;
    nx::Buffer m_buffer;
};
