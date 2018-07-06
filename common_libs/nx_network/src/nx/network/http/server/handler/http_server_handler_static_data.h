#pragma once

#include "../abstract_http_request_handler.h"

namespace nx {
namespace network {
namespace http {
namespace server {
namespace handler {

class NX_NETWORK_API StaticData:
    public AbstractHttpRequestHandler
{
public:
    StaticData(const nx::network::http::StringType& mimeType, QByteArray response);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    const nx::network::http::StringType m_mimeType;
    const QByteArray m_response;
};

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
