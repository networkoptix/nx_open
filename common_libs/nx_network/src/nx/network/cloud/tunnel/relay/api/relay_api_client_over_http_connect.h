#pragma once

#include "relay_api_client_over_http_upgrade.h"

namespace nx::cloud::relay::api {

class NX_NETWORK_API ClientImplUsingHttpConnect:
    public ClientOverHttpUpgrade
{
    using base_type = ClientOverHttpUpgrade;

public:
    ClientImplUsingHttpConnect(const nx::utils::Url& baseUrl);

    virtual void beginListening(
        const nx::String& peerName,
        BeginListeningHandler completionHandler) override;

private:
    void processBeginListeningResponse(
        std::list<std::unique_ptr<network::aio::BasicPollable>>::iterator httpClientIter,
        BeginListeningHandler completionHandler);
};

} // namespace nx::cloud::relay::api
