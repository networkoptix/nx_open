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
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        m_func(
            connection,
            std::move(authInfo),
            std::move(request),
            response,
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
