#include "create_get_post_tunnel_handler.h"

namespace nx::cloud::relay::view {

const char* CreatePostGetTunnelHandler::kPath = api::kServerTunnelPath;

CreatePostGetTunnelHandler::CreatePostGetTunnelHandler(
    GetPostTunnelProcessor* tunnelProcessor)
    :
    m_tunnelProcessor(tunnelProcessor)
{
}

void CreatePostGetTunnelHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (requestPathParams().empty())
        return completionHandler(nx::network::http::StatusCode::badRequest);

    auto requestResult = m_tunnelProcessor->processOpenTunnelRequest(
        connection,
        requestPathParams()[0].toStdString(),
        request,
        response);

    completionHandler(std::move(requestResult));
}

} // namespace nx::cloud::relay::view
