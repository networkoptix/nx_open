#pragma once

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "get_post_tunnel_processor.h"

namespace nx::cloud::relay::view {

class CreatePostGetTunnelHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    static const char* kPath;

    CreatePostGetTunnelHandler(
        GetPostTunnelProcessor* tunnelProcessor);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    GetPostTunnelProcessor* m_tunnelProcessor;
};

} // namespace nx::cloud::relay::view
