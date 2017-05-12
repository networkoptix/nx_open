#include "http_handlers.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

const char* BeginListeningHandler::kPath = api::kServerIncomingConnectionsPath;

BeginListeningHandler::BeginListeningHandler(
    controller::AbstractConnectSessionManager* connectSessionManager)
    :
    base_type(
        connectSessionManager,
        &controller::AbstractConnectSessionManager::beginListening)
{
}

api::BeginListeningRequest BeginListeningHandler::prepareRequestData()
{
    api::BeginListeningRequest inputData;
    inputData.peerName = requestPathParams()[0].toStdString();
    return inputData;
}

//-------------------------------------------------------------------------------------------------

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
    api::CreateClientSessionRequest* request)
{
    request->targetPeerName = requestPathParams()[0].toStdString();
}

//-------------------------------------------------------------------------------------------------

const char* ConnectToPeerHandler::kPath = api::kClientSessionConnectionsPath;

ConnectToPeerHandler::ConnectToPeerHandler(
    controller::AbstractConnectSessionManager* connectSessionManager)
    :
    base_type(
        connectSessionManager,
        &controller::AbstractConnectSessionManager::connectToPeer)
{
}

api::ConnectToPeerRequest ConnectToPeerHandler::prepareRequestData()
{
    api::ConnectToPeerRequest inputData;
    inputData.sessionId = requestPathParams()[0].toStdString();
    return inputData;
}

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
