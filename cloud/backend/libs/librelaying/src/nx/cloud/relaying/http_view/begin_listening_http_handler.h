#pragma once

#include "basic_http_handler.h"
#include "../listening_peer_manager.h"

namespace nx {
namespace cloud {
namespace relaying {

class NX_RELAYING_API BeginListeningUsingConnectMethodHandler:
    public BasicHandlerWithoutRequestBody<
        AbstractListeningPeerManager,
        relay::api::BeginListeningRequest,
        AbstractListeningPeerManager::BeginListeningHandler,
        relay::api::BeginListeningResponse>
{
    using base_type =
        BasicHandlerWithoutRequestBody<
            AbstractListeningPeerManager,
            relay::api::BeginListeningRequest,
            AbstractListeningPeerManager::BeginListeningHandler,
            relay::api::BeginListeningResponse>;

public:
    static const char* kPath;

    BeginListeningUsingConnectMethodHandler(
        AbstractListeningPeerManager* listeningPeerManager);

protected:
    virtual relay::api::BeginListeningRequest prepareRequestData(
        const nx::network::http::RequestContext& requestContext) override;
};

} // namespace relaying
} // namespace cloud
} // namespace nx
