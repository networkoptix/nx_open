// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_api_notifications.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::cloud::relay::api {

static constexpr char kClientEndpoint[] = "X-Nx-Client-Endpoint";
static constexpr char kOpenTunnel[] = "OPEN_TUNNEL";

void OpenTunnelNotification::setClientPeerName(std::string name)
{
    m_clientPeerName = std::move(name);
}

const std::string& OpenTunnelNotification::clientPeerName() const
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
    message.request->requestLine.method = kOpenTunnel;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        m_clientPeerName);
    message.request->headers.emplace(
        kClientEndpoint, m_clientEndpoint.toString());
    return message;
}

bool OpenTunnelNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kOpenTunnel)
        return false;

    auto path = message.request->requestLine.url.path().toStdString();
    if (!nx::utils::startsWith(path, nx::cloud::relay::api::kRelayClientPathPrefix))
        return false;
    path.erase(0, strlen(nx::cloud::relay::api::kRelayClientPathPrefix));
    m_clientPeerName = path;

    auto clientEndpointIter = message.request->headers.find(kClientEndpoint);
    if (clientEndpointIter == message.request->headers.end())
        return false;
    m_clientEndpoint = network::SocketAddress(clientEndpointIter->second);

    return true;
}

//-------------------------------------------------------------------------------------------------

static constexpr char kKeepAlive[] = "KEEP_ALIVE";
static constexpr char kConnectionPath[] = "connection";

nx::network::http::Message KeepAliveNotification::toHttpMessage() const
{
    nx::network::http::Message message(nx::network::http::MessageType::request);
    message.request->requestLine.method = kKeepAlive;
    message.request->requestLine.version = {kRelayProtocolName, kRelayProtocolVersion};
    message.request->requestLine.url = nx::network::url::joinPath(
        nx::cloud::relay::api::kRelayClientPathPrefix,
        kConnectionPath);
    return message;
}

bool KeepAliveNotification::parse(const nx::network::http::Message& message)
{
    if (message.type != nx::network::http::MessageType::request)
        return false;

    if (message.request->requestLine.method != kKeepAlive)
        return false;

    if (message.request->requestLine.url.path().toStdString() !=
        nx::network::url::joinPath(
            nx::cloud::relay::api::kRelayClientPathPrefix,
            kConnectionPath))
    {
        return false;
    }

    return true;
}

} // namespace nx::cloud::relay::api
