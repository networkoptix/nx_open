#include "http_handlers.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

const char* BeginListeningHandler::kPath = api::kServerIncomingConnectionsPath;

BeginListeningHandler::BeginListeningHandler(
    controller::AbstractListeningPeerManager* listeningPeerManager)
    :
    base_type(
        listeningPeerManager,
        &controller::AbstractListeningPeerManager::beginListening)
{
}

api::BeginListeningRequest BeginListeningHandler::prepareRequestData(
    nx_http::HttpServerConnection* const /*connection*/,
    const nx_http::Request& /*httpRequest*/)
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
    nx_http::HttpServerConnection* const /*connection*/,
    const nx_http::Request& /*httpRequest*/,
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

controller::ConnectToPeerRequestEx ConnectToPeerHandler::prepareRequestData(
    nx_http::HttpServerConnection* const connection,
    const nx_http::Request& /*httpRequest*/)
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
