#pragma once

#include <nx/network/buffer.h>
#include <nx/network/http/httptypes.h>
#include <nx/network/socket_common.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

class NX_NETWORK_API OpenTunnelNotification
{
public:
    void setClientPeerName(nx::String name);
    const nx::String& clientPeerName() const;

    void setClientEndpoint(SocketAddress endpoint);
    const SocketAddress& clientEndpoint() const;

    nx_http::Message toHttpMessage() const;
    bool parse(const nx_http::Message& message);

private:
    nx::String m_clientPeerName;
    SocketAddress m_clientEndpoint;
};

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
