// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>

namespace nx::cloud::relay::api {

class NX_NETWORK_API OpenTunnelNotification
{
public:
    static constexpr std::string_view kHttpMethod = "OPEN_TUNNEL";

    void setClientPeerName(std::string name);
    const std::string& clientPeerName() const;

    void setClientEndpoint(network::SocketAddress endpoint);
    const network::SocketAddress& clientEndpoint() const;

    nx::network::http::Message toHttpMessage() const;
    bool parse(const nx::network::http::Message& message);

private:
    std::string m_clientPeerName;
    network::SocketAddress m_clientEndpoint;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API KeepAliveNotification
{
public:
    static constexpr std::string_view kHttpMethod = "KEEP_ALIVE";

    nx::network::http::Message toHttpMessage() const;

    /**
     * Example: "UPDATE /relay/client/connection NXRELAY/0.1".
     */
    bool parse(const nx::network::http::Message& message);
};

} // namespace nx::cloud::relay::api
