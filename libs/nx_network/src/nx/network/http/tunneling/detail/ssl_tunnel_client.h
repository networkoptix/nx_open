// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "connection_upgrade_tunnel_client.h"

namespace nx::network::http::tunneling::detail {

class NX_NETWORK_API SslTunnelClient:
    public ConnectionUpgradeTunnelClient
{
    using base_type = ConnectionUpgradeTunnelClient;

public:
    SslTunnelClient(
        const nx::utils::Url& baseTunnelUrl,
        const ConnectOptions& options,
        ClientFeedbackFunction clientFeedbackFunction);

private:
    static nx::utils::Url convertToHttpsUrl(nx::utils::Url httpUrl);
};

} // namespace nx::network::http::tunneling::detail
