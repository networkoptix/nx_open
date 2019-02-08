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
    const nx::network::http::RequestContext& requestContext,
    api::CreateClientSessionRequest* request)
{
    request->targetPeerName = 
        requestContext.requestPathParams.getByName(api::kServerIdName);
}

void CreateClientSessionHandler::beforeReportingResponse(
    api::ResultCode resultCode,
    const api::CreateClientSessionResponse& apiResponse)
{
    if (resultCode == api::ResultCode::needRedirect &&
        !apiResponse.actualRelayUrl.empty())
    {
        response()->headers.emplace("Location", apiResponse.actualRelayUrl.c_str());
    }
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

controller::ConnectToPeerRequestEx 
    ConnectToListeningPeerWithHttpUpgradeHandler::prepareRequestData(
        const nx::network::http::RequestContext& requestContext)
{
    controller::ConnectToPeerRequestEx inputData;
    inputData.clientEndpoint = requestContext.connection->socket()->getForeignAddress();
    inputData.sessionId = requestContext.requestPathParams.getByName(api::kSessionIdName);
    return inputData;
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
