#include "create_get_post_tunnel_handler.h"

namespace nx::cloud::relay::view {

const char* CreateGetPostServerTunnelHandler::kPath = api::kServerTunnelPath;

CreateGetPostServerTunnelHandler::CreateGetPostServerTunnelHandler(
    GetPostServerTunnelProcessor* tunnelProcessor)
    :
    m_tunnelProcessor(tunnelProcessor)
{
}

void CreateGetPostServerTunnelHandler::processRequest(
    nx::network::http::HttpServerConnection* const /*connection*/,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    if (requestPathParams().empty())
        return completionHandler(nx::network::http::StatusCode::badRequest);

    auto requestResult = m_tunnelProcessor->processOpenTunnelRequest(
        requestPathParams()[0].toStdString(),
        request,
        response);

    completionHandler(std::move(requestResult));
}

//-------------------------------------------------------------------------------------------------

const char* CreateGetPostClientTunnelHandler::kPath = api::kClientGetPostTunnelPath;

CreateGetPostClientTunnelHandler::CreateGetPostClientTunnelHandler(
    controller::AbstractConnectSessionManager* connectSessionManager,
    GetPostClientTunnelProcessor* tunnelProcessor)
    :
    m_connectSessionManager(connectSessionManager),
    m_tunnelProcessor(tunnelProcessor)
{
}

void CreateGetPostClientTunnelHandler::processRequest(
    nx::network::http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer /*authInfo*/,
    nx::network::http::Request request,
    nx::network::http::Response* const response,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    using namespace std::placeholders;

    if (requestPathParams().empty())
        return completionHandler(nx::network::http::StatusCode::badRequest);

    m_sessionId = requestPathParams()[0].toStdString();

    controller::ConnectToPeerRequestEx inputData;
    inputData.clientEndpoint = connection->socket()->getForeignAddress();
    inputData.sessionId = m_sessionId;

    m_completionHandler = std::move(completionHandler);
    m_request = std::move(request);
    m_response = response;

    m_connectSessionManager->connectToPeer(
        inputData,
        std::bind(&CreateGetPostClientTunnelHandler::connectToPeerFinished, this,
            _1, _2));
}

void CreateGetPostClientTunnelHandler::connectToPeerFinished(
    api::ResultCode resultCode,
    controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc)
{
    if (resultCode != api::ResultCode::ok)
    {
        nx::utils::swapAndCall(
            m_completionHandler,
            relay::api::resultCodeToFusionRequestResult(resultCode).httpStatusCode());
        return;
    }

    auto requestResult = m_tunnelProcessor->processOpenTunnelRequest(
        std::move(startRelayingFunc),
        m_request,
        m_response);

    nx::utils::swapAndCall(
        m_completionHandler,
        std::move(requestResult));
}

} // namespace nx::cloud::relay::view
