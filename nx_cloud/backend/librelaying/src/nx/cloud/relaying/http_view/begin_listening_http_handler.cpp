#include "begin_listening_http_handler.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relaying {

const char* BeginListeningHandler::kPath = relay::api::kServerIncomingConnectionsPath;

BeginListeningHandler::BeginListeningHandler(
    AbstractListeningPeerManager* listeningPeerManager)
    :
    base_type(
        listeningPeerManager,
        &relaying::AbstractListeningPeerManager::beginListening)
{
    addResultCodeToHttpStatusConversion(
        relay::api::ResultCode::ok,
        nx::network::http::StatusCode::switchingProtocols);
}

relay::api::BeginListeningRequest BeginListeningHandler::prepareRequestData(
    nx::network::http::HttpServerConnection* const /*connection*/,
    const nx::network::http::Request& /*httpRequest*/)
{
    relay::api::BeginListeningRequest inputData;
    inputData.peerName = requestPathParams()[0].toStdString();
    return inputData;
}

//-------------------------------------------------------------------------------------------------

const char* BeginListeningUsingConnectMethodHandler::kPath = "";

BeginListeningUsingConnectMethodHandler::BeginListeningUsingConnectMethodHandler(
    AbstractListeningPeerManager* listeningPeerManager)
    :
    base_type(
        listeningPeerManager,
        &relaying::AbstractListeningPeerManager::beginListening)
{
}

relay::api::BeginListeningRequest BeginListeningUsingConnectMethodHandler::prepareRequestData(
    nx::network::http::HttpServerConnection* const /*connection*/,
    const nx::network::http::Request& httpRequest)
{
    relay::api::BeginListeningRequest inputData;
    inputData.peerName = httpRequest.requestLine.url.path().toStdString();
    return inputData;
}

} // namespace relaying
} // namespace cloud
} // namespace nx
