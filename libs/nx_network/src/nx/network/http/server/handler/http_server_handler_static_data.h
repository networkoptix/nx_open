// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../abstract_http_request_handler.h"

namespace nx::network::http::server::handler {

class NX_NETWORK_API StaticData:
    public RequestHandlerWithContext
{
public:
    StaticData(const std::string& mimeType, nx::Buffer response);

    virtual void processRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override;

private:
    const std::string m_mimeType;
    const nx::Buffer m_response;
};

} // namespace nx::network::http::server::handler
