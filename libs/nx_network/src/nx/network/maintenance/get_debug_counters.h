#pragma once

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::network::maintenance {

class GetDebugCounters:
    public http::AbstractHttpRequestHandler
{
protected:
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) override;
};

} // namespace nx::network::maintenance
