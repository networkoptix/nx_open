#include "client_connection_tunneling.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_data_types.h>

namespace nx::cloud::relay::view {

ClientConnectionTunnelingServer::ClientConnectionTunnelingServer(
    controller::AbstractConnectSessionManager* connectSessionManager)
    :
    m_connectSessionManager(connectSessionManager),
    m_tunnelingServer(
        std::bind(&ClientConnectionTunnelingServer::saveNewTunnel, this,
            std::placeholders::_1, std::placeholders::_2),
        this)
{
}

void ClientConnectionTunnelingServer::registerHandlers(
    const std::string& basePath,
    network::http::server::rest::MessageDispatcher* messageDispatcher)
{
    m_tunnelingServer.registerRequestHandlers(
        basePath,
        messageDispatcher);
}

void ClientConnectionTunnelingServer::authorize(
    const nx::network::http::RequestContext* requestContext,
    CompletionHandler completionHandler)
{
    using namespace std::placeholders;

    if (requestContext->requestPathParams.empty())
    {
        return completionHandler(
            nx::network::http::StatusCode::badRequest,
            nullptr);
    }

    controller::ConnectToPeerRequestEx inputData;
    inputData.clientEndpoint = requestContext->connection->socket()->getForeignAddress();
    inputData.sessionId = requestContext->requestPathParams[0].toStdString();

    m_connectSessionManager->connectToPeer(
        inputData,
        [this, completionHandler = std::move(completionHandler)](
            api::ResultCode resultCode,
            controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc) mutable
        {
            connectToPeerFinished(
                resultCode,
                std::move(startRelayingFunc),
                std::move(completionHandler));
        });
}

void ClientConnectionTunnelingServer::connectToPeerFinished(
    api::ResultCode resultCode,
    controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
    CompletionHandler completionHandler)
{
    if (resultCode != api::ResultCode::ok)
    {
        nx::utils::swapAndCall(
            completionHandler,
            relay::api::resultCodeToFusionRequestResult(resultCode).httpStatusCode(),
            nullptr);
        return;
    }

    nx::utils::swapAndCall(
        completionHandler,
        network::http::StatusCode::ok,
        std::move(startRelayingFunc));
}

void ClientConnectionTunnelingServer::saveNewTunnel(
    controller::AbstractConnectSessionManager::StartRelayingFunc startRelayingFunc,
    std::unique_ptr<network::AbstractStreamSocket> connection)
{
    startRelayingFunc(std::move(connection));
}

} // namespace nx::cloud::relay::view
