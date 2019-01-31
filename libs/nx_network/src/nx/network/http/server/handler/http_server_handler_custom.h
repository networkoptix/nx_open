#pragma once

#include "../abstract_http_request_handler.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

template<typename Func>
class CustomRequestHandler:
    public AbstractHttpRequestHandler
{
public:
    CustomRequestHandler(Func func):
        m_func(func)
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

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
