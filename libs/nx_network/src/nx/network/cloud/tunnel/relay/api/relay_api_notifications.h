#pragma once

#include <nx/network/buffer.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>

namespace nx::cloud::relay::api {

class NX_NETWORK_API OpenTunnelNotification
{
public:
    static constexpr char kHttpMethod[] = "OPEN_TUNNEL";

    void setClientPeerName(nx::String name);
    const nx::String& clientPeerName() const;

    void setClientEndpoint(network::SocketAddress endpoint);
    const network::SocketAddress& clientEndpoint() const;

    nx::network::http::Message toHttpMessage() const;
    bool parse(const nx::network::http::Message& message);

private:
    nx::String m_clientPeerName;
    network::SocketAddress m_clientEndpoint;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API KeepAliveNotification
{
public:
    static constexpr char kHttpMethod[] = "KEEP_ALIVE";

    nx::network::http::Message toHttpMessage() const;

    /**
     * Example: "UPDATE /relay/client/connection NXRELAY/0.1".
     */
    bool parse(const nx::network::http::Message& message);
};

} // namespace nx::cloud::relay::api
