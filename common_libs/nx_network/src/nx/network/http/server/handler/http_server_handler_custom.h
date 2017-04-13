#pragma once

#include "../abstract_http_request_handler.h"

namespace nx_http {
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
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler) override
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
} // namespace nx_http
