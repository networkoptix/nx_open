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
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override;

private:
    const StringType m_mimeType;
    const QByteArray m_response;
};

} // namespace handler
} // namespace server
} // namespace nx
} // namespace network
} // namespace http
