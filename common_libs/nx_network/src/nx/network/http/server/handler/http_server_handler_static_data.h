#pragma once

#include "../abstract_http_request_handler.h"

namespace nx_http {
namespace server {
namespace handler {

class NX_NETWORK_API StaticData:
    public AbstractHttpRequestHandler
{
public:
    StaticData(const nx_http::StringType& mimeType, QByteArray response);

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override;

private:
    const nx_http::StringType m_mimeType;
    const QByteArray m_response;
};

} // namespace handler
} // namespace server
} // namespace nx_http
