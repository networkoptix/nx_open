// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../abstract_http_request_handler.h"

namespace nx::network::http::server::handler {

class NX_NETWORK_API Redirect:
    public RequestHandlerWithContext
{
public:
    Redirect(const nx::utils::Url& actualLocation);

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const nx::utils::Url m_actualLocation;
};

} // namespace nx::network::http::server::handler
