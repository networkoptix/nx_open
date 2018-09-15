#pragma once

#include "basic_tunnel_client.h"
#include "../../http_async_client.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API ConnectionUpgradeTunnelClient:
    public BasicTunnelClient
{
    using base_type = BasicTunnelClient;

public:
    ConnectionUpgradeTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

private:
    nx::utils::Url m_tunnelUrl;
    http::Response m_openTunnelResponse;

    void processOpenTunnelResponse();
};

} // namespace nx::network::http::tunneling::detail
