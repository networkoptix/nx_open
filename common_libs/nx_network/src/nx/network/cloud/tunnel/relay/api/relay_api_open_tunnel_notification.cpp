#include "relay_api_open_tunnel_notification.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>

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

void OpenTunnelNotification::setClientEndpoint(network::SocketAddress endpoint)
{
    m_clientEndpoint = std::move(endpoint);
}

const network::SocketAddress& OpenTunnelNotification::clientEndpoint() const
{
    return m_clientEndpoint;
}

nx::network::http::Message OpenTunnelNotification::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::request);
    message.request->requestLine.method = "OPEN_TUNNEL";
    message.request->requestLine.version.protocol = "NXRELAY";
    message.request->requestLine.version.version = "0.1";
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        m_clientPeerName.toStdString());
    message.request->headers.emplace(
        kClientEndpoint, m_clientEndpoint.toString().toUtf8());
    return message;
}

bool OpenTunnelNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    auto path = message.request->requestLine.url.path().toUtf8();
    if (!path.startsWith(nx::cloud::relay::api::kRelayClientPathPrefix))
        return false;
    path.remove(0, strlen(nx::cloud::relay::api::kRelayClientPathPrefix));
    m_clientPeerName = path;

    auto clientEndpointIter = message.request->headers.find(kClientEndpoint);
    if (clientEndpointIter == message.request->headers.end())
        return false;
    m_clientEndpoint = network::SocketAddress(clientEndpointIter->second);

    return true;
}

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
