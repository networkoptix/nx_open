#pragma once

#include "../abstract_http_request_handler.h"

namespace nx_http {
namespace server {
namespace handler {

class NX_NETWORK_API Redirect:
    public AbstractHttpRequestHandler
{
public:
    Redirect(const nx::utils::Url& actualLocation);

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    const nx::utils::Url m_actualLocation;
};

} // namespace handler
} // namespace server
} // namespace nx_http
