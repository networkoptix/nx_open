#pragma once

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/server/abstract_http_request_handler.h>

#include "get_post_tunnel_processor.h"
#include "../controller/connect_session_manager.h"

namespace nx::cloud::relay::view {

class CreateGetPostServerTunnelHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    static const char* kPath;

    CreateGetPostServerTunnelHandler(
        GetPostServerTunnelProcessor* tunnelProcessor);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    GetPostServerTunnelProcessor * m_tunnelProcessor;
};

//-------------------------------------------------------------------------------------------------

class CreateGetPostClientTunnelHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    static const char* kPath;

    CreateGetPostClientTunnelHandler(
        controller::AbstractConnectSessionManager* connectSessionManager,
        GetPostClientTunnelProcessor* tunnelProcessor);

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler) override;

private:
    controller::AbstractConnectSessionManager* m_connectSessionManager;
    GetPostClientTunnelProcessor* m_tunnelProcessor;
    std::string m_sessionId;
    nx::network::http::RequestProcessedHandler m_completionHandler;
    nx::network::http::Request m_request;
    nx::network::http::Response* m_response = nullptr;

    void connectToPeerFinished(
        api::ResultCode resultCode,
        controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc);
};

} // namespace nx::cloud::relay::view
