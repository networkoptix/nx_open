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
        nx::network::http::HttpServerConnection* const connection,
        const nx::network::http::Request& httpRequest) override;
};

} // namespace relaying
} // namespace cloud
} // namespace nx
