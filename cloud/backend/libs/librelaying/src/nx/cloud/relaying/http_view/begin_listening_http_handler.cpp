#include "begin_listening_http_handler.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relaying {

const char* BeginListeningUsingConnectMethodHandler::kPath = "";

BeginListeningUsingConnectMethodHandler::BeginListeningUsingConnectMethodHandler(
    AbstractListeningPeerManager* listeningPeerManager)
    :
    base_type(
        listeningPeerManager,
        &relaying::AbstractListeningPeerManager::beginListening)
{
}

relay::api::BeginListeningRequest
    BeginListeningUsingConnectMethodHandler::prepareRequestData(
        const nx::network::http::RequestContext& requestContext)
{
    relay::api::BeginListeningRequest inputData;
    inputData.peerName = requestContext.request.requestLine.url.path().toStdString();
    return inputData;
}

} // namespace relaying
} // namespace cloud
} // namespace nx
