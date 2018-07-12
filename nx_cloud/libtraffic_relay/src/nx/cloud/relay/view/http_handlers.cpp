#include "http_handlers.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

const char* CreateClientSessionHandler::kPath = api::kServerClientSessionsPath;

CreateClientSessionHandler::CreateClientSessionHandler(
    controller::AbstractConnectSessionManager* connectSessionManager)
    :
    base_type(
        connectSessionManager,
        &controller::AbstractConnectSessionManager::createClientSession)
{
}

void CreateClientSessionHandler::prepareRequestData(
    nx::network::http::HttpServerConnection* const /*connection*/,
    const nx::network::http::Request& /*httpRequest*/,
    api::CreateClientSessionRequest* request)
{
    request->targetPeerName = requestPathParams()[0].toStdString();
}

//-------------------------------------------------------------------------------------------------

const char* ConnectToListeningPeerWithHttpUpgradeHandler::kPath =
    api::kClientSessionConnectionsPath;

ConnectToListeningPeerWithHttpUpgradeHandler::ConnectToListeningPeerWithHttpUpgradeHandler(
    controller::AbstractConnectSessionManager* connectSessionManager)
    :
    base_type(
        connectSessionManager,
        &controller::AbstractConnectSessionManager::connectToPeer)
{
    addResultCodeToHttpStatusConversion(
        relay::api::ResultCode::ok,
        nx::network::http::StatusCode::switchingProtocols);
}

controller::ConnectToPeerRequestEx ConnectToListeningPeerWithHttpUpgradeHandler::prepareRequestData(
    nx::network::http::HttpServerConnection* const connection,
    const nx::network::http::Request& /*httpRequest*/)
{
    controller::ConnectToPeerRequestEx inputData;
    inputData.clientEndpoint = connection->socket()->getForeignAddress();
    inputData.sessionId = requestPathParams()[0].toStdString();
    return inputData;
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
