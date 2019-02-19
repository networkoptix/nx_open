#include "relay_api_notifications.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::cloud::relay::api {

static constexpr char kClientEndpoint[] = "X-Nx-Client-Endpoint";

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
    message.request->requestLine.method = kHttpMethod;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        m_clientPeerName.toStdString().c_str());
    message.request->headers.emplace(
        kClientEndpoint, m_clientEndpoint.toString().toUtf8());
    return message;
}

bool OpenTunnelNotification::parse(const nx_http::Message& message)
{
    if (message.type != nx_http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kHttpMethod)
        return false;

    auto path = message.request->requestLine.url.path().toUtf8();
    if (!path.startsWith(nx::cloud::relay::api::kRelayClientPathPrefix))
        return false;
    path.remove(0, strlen(nx::cloud::relay::api::kRelayClientPathPrefix));
    m_clientPeerName = path;

    auto clientEndpointIter = message.request->headers.find(kClientEndpoint);
    if (clientEndpointIter == message.request->headers.end())
        return false;
    m_clientEndpoint = SocketAddress(clientEndpointIter->second);

    return true;
}

//-------------------------------------------------------------------------------------------------

static constexpr char kConnectionPath[] = "connection";

nx_http::Message KeepAliveNotification::toHttpMessage() const
{
    nx_http::Message message(nx_http::MessageType::request);
    message.request->requestLine.method = kHttpMethod;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        kConnectionPath);
    return message;
}

bool KeepAliveNotification::parse(const nx_http::Message& message)
{
    if (message.type != nx_http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kHttpMethod)
        return false;

    if (message.request->requestLine.url.path() !=
        nx::network::url::joinPath(
            nx::cloud::relay::api::kRelayClientPathPrefix,
            kConnectionPath))
    {
        return false;
    }

    return true;
}

} // namespace nx::cloud::relay::api
