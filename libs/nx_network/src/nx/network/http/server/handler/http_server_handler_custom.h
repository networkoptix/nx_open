// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../abstract_http_request_handler.h"

namespace nx::network::http::server::handler {

template<typename Func>
class CustomRequestHandler:
    public RequestHandlerWithContext
{
public:
    CustomRequestHandler(Func func):
        m_func(std::move(func))
    {
    }

    virtual void processRequest(
        RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        m_func(
            std::move(requestContext),
            std::move(completionHandler));
    }

private:
    Func m_func;
};

} // namespace nx::network::http::server::handler
