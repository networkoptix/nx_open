#pragma once

#include "connection_upgrade_tunnel_client.h"

namespace nx_http::tunneling::detail {

class NX_NETWORK_API SslTunnelClient:
    public ConnectionUpgradeTunnelClient
{
    using base_type = ConnectionUpgradeTunnelClient;

public:
    SslTunnelClient(
        const QUrl& baseTunnelUrl,
        ClientFeedbackFunction clientFeedbackFunction);

private:
    static QUrl convertToHttpsUrl(QUrl httpUrl);
};

} // namespace nx_http::tunneling::detail
