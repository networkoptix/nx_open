// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::network::http {

/**
 * Sends specified buffer forever.
 */
class RepeatingBufferSender:
    public nx::network::http::RequestHandlerWithContext
{
public:
    RepeatingBufferSender(const std::string& mimeType, nx::Buffer buffer);

    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const std::string m_mimeType;
    nx::Buffer m_buffer;
};

} // namespace nx::network::http
