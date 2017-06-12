#include "relay_api_open_tunnel_notification.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/http/rest/http_rest_client.h>

namespace nx {
namespace cloud {
namespace relay {
namespace api {

static const nx::String kClientEndpoint("X-Nx-Client-Endpoint");

void OpenTunnelNotification::setClientPeerName(nx::String name)
{
    m_clientPeerName = std::move(name);
}

const nx::String& OpenTunnelNotification::clientPeerName() const
{
    return m_clientPeerName;
}

void OpenTunnelNotification::setClientEndpoint(SocketAddress endpoint)
{
    m_clientEndpoint = std::move(endpoint);
}

const SocketAddress& OpenTunnelNotification::clientEndpoint() const
{
    return m_clientEndpoint;
}

nx_http::Message OpenTunnelNotification::toHttpMessage() const
{
    nx_http::Message message(nx_http::MessageType::request);
    message.request->requestLine.method = "OPEN_TUNNEL";
    message.request->requestLine.version.protocol = "NXRELAY";
    message.request->requestLine.version.version = "0.1";
    message.request->requestLine.url =
        nx_http::rest::substituteParameters(
            nx::cloud::relay::api::kRelayClientPath, { m_clientPeerName });
    message.request->headers.emplace(
        kClientEndpoint, m_clientEndpoint.toString().toUtf8());
    return message;
}

bool OpenTunnelNotification::parse(const nx_http::Message& message)
{
    if (message.type != nx_http::MessageType::request)
        return false;

    auto clientEndpointIter = message.request->headers.find(kClientEndpoint);
    if (clientEndpointIter == message.request->headers.end())
        return false;
    m_clientEndpoint = SocketAddress(clientEndpointIter->second);

    return true;
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
