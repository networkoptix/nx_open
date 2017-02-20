#include "http_server_handler_custom_func.h"

#include <memory>

namespace nx_http {
namespace server {
namespace handler {

CustomFunc::CustomFunc(CustomFunc::ProcessHttpRequestFunc func):
    m_func(std::move(func))
{
}

void CustomFunc::processRequest(
    nx_http::HttpServerConnection* const connection,
    stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::RequestProcessedHandler completionHandler)
{
    m_func(
        connection,
        std::move(authInfo),
        std::move(request),
        response,
        std::move(completionHandler));
}

} // namespace handler
} // namespace server
} // namespace nx_http
