// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/std/optional.h>

#include "../../http_async_client.h"
#include "base_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API ConnectionUpgradeTunnelClient:
    public BaseTunnelClient
{
    using base_type = BaseTunnelClient;

public:
    ConnectionUpgradeTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        const ConnectOptions& options,
        ClientFeedbackFunction clientFeedbackFunction);

    virtual void setTimeout(std::optional<std::chrono::milliseconds> timeout) override;

    virtual void openTunnel(
        OpenTunnelCompletionHandler completionHandler) override;

    virtual const Response& response() const override;

private:
    nx::utils::Url m_tunnelUrl;
    http::Response m_openTunnelResponse;
    std::optional<std::chrono::milliseconds> m_timeout;
    bool m_isConnectionTestRequested = false;

    void processOpenTunnelResponse();
};

} // namespace nx::network::http::tunneling::detail
