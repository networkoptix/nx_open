#pragma once

#include <nx/utils/std/optional.h>

#include "base_tunnel_client.h"
#include "../../http_async_client.h"

namespace nx_http::tunneling::detail {

class NX_NETWORK_API ConnectionUpgradeTunnelClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    ConnectionUpgradeTunnelClient(
        const QUrl& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void setTimeout(std::chrono::milliseconds timeout) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

private:
    QUrl m_tunnelUrl;
    nx_http::Response m_openTunnelResponse;
    std::optional<std::chrono::milliseconds> m_timeout;

    void processOpenTunnelResponse();
};

} // namespace nx_http::tunneling::detail
