#pragma once

#include "relay_api_basic_client.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API ClientOverHttpUpgrade:
    public BasicClient
{
    using base_type = BasicClient;

public:
    ClientOverHttpUpgrade(
        const nx::utils::Url& baseUrl,
        ClientFeedbackFunction /*feedbackFunction*/);

    virtual void beginListening(
        const nx::String& peerName,
        BeginListeningHandler completionHandler) override;

    virtual void openConnectionToTheTargetHost(
        const nx::String& sessionId,
        OpenRelayConnectionHandler handler) override;
};

} // namespace nx::cloud::relay::api
