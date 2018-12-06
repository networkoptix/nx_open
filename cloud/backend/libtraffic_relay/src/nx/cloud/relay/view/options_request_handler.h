#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::cloud::relay::view {

class OptionsRequestHandler:
    public network::http::AbstractHttpRequestHandler
{
public:
    static const char* kPath;

protected:
    virtual void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;
};

} // namespace nx::cloud::relay::view
