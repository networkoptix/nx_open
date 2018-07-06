#pragma once

#include "../abstract_http_request_handler.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

class NX_NETWORK_API Redirect:
    public AbstractHttpRequestHandler
{
public:
    Redirect(const nx::utils::Url& actualLocation);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const nx::utils::Url m_actualLocation;
};

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
