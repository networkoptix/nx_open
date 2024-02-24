// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "local_proxy_server.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/socket_common.h>
#include <nx/network/url/url_builder.h>
#include <nx/network/websocket/websocket_handshake.h>
#include <nx/utils/url.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

template<>
nx::vms::client::desktop::LocalProxyServer*
    Singleton<nx::vms::client::desktop::LocalProxyServer>::s_instance = nullptr;

namespace nx::vms::client::desktop {

using namespace nx::network;
using namespace nx::network::http;

/**
 * Implements custom logic for proxying a connection through VMS server. The instance of this class
 * is created for each incoming connection.
 */
class VmsServerConnector:
    public nx::network::socks5::AbstractTunnelConnector,
    public SystemContextAware,
    public core::RemoteConnectionAware
{
    using base_type = nx::network::socks5::AbstractTunnelConnector;

public:
    // FIXME: #sivanov Use actual system context.
    VmsServerConnector():
        SystemContextAware(appContext()->currentSystemContext())
    {
    }

    virtual bool hasAuth() const override { return true; }
    virtual bool auth(const std::string& user, const std::string& password) override;
    virtual void connectTo(const SocketAddress& address, DoneCallback onDone) override;

    virtual ~VmsServerConnector() = default;

protected:
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        if (m_httpClient)
            m_httpClient->bindToAioThread(aioThread);
    }

    virtual void stopWhileInAioThread() override
    {
        base_type::stopWhileInAioThread();
        m_httpClient.reset();
    }

private:
    DoneCallback m_onDone;

    std::unique_ptr<AsyncClient> m_httpClient;

    nx::Uuid m_exitNodeId;
    nx::Uuid m_resourceId;
};

bool VmsServerConnector::auth(const std::string& user, const std::string& password)
{
    const auto resourceId = nx::Uuid::fromStringSafe(user);

    const auto resource = resourcePool()->getResourceById(resourceId);

    if (resource.dynamicCast<QnMediaServerResource>())
    {
        // Using a server id as a user name allows to tunnel the connection to any address via
        // the specified server. However, this requires `allowThirdPartyProxy` config option to be
        // enabled on the server.
        m_exitNodeId = resourceId;
    }
    else if (resource.dynamicCast<QnNetworkResource>() || resource.dynamicCast<QnWebPageResource>())
    {
        m_exitNodeId = resource->getParentId();
        m_resourceId = resourceId;
    }
    else if (resource.dynamicCast<nx::vms::common::AnalyticsEngineResource>())
    {
        m_exitNodeId = serverId();
        m_resourceId = resourceId;
    }

    return !m_exitNodeId.isNull() && password == LocalProxyServer::instance()->password();
}

void VmsServerConnector::connectTo(const SocketAddress& address, DoneCallback onDone)
{
    m_onDone = std::move(onDone);

    m_httpClient = std::make_unique<AsyncClient>(ssl::kAcceptAnyCertificate);
    // We should not force any timeouts on SOCKS connection.
    m_httpClient->setTimeouts(AsyncClient::kInfiniteTimeouts);
    m_httpClient->bindToAioThread(getAioThread());

    // Build a special url which enables VMS server to tunnel connection data inside web socket.
    const auto url = url::Builder()
        .setScheme(nx::network::http::kSecureUrlSchemeName)
        .setPath(nx::format("/proxy/socks5/%1:%2", address.address.toString(), address.port))
        .setEndpoint(connectionAddress())
        .toUrl();

    // Set credentials from current server connection.
    m_httpClient->setCredentials(connectionCredentials());

    // Prepare the request headers and send the request.
    HttpHeaders headers;
    headers.emplace(Qn::SERVER_GUID_HEADER_NAME, m_exitNodeId.toStdString());
    if (!m_resourceId.isNull())
        headers.emplace(Qn::WEB_RESOURCE_ID_HEADER_NAME, m_resourceId.toStdString());

    m_httpClient->setAdditionalHeaders(headers);
    m_httpClient->doUpgrade(
        url,
        nx::network::http::Method::get,
        nx::network::websocket::kWebsocketProtocolName,
        [this]
        {
            // The only valid response is 101 Switching Protocols indicating that the connection is
            // ready for proxying the data.
            if (m_httpClient->hasRequestSucceeded())
            {
                m_onDone(SystemError::noError, m_httpClient->takeSocket());
                return;
            }

            // TODO: More error codes can be reported but Chromium doesn't show them.
            m_onDone(SystemError::hostNotFound, nullptr);
        });
}

LocalProxyServer::LocalProxyServer(): m_password(nx::Uuid::createUuid().toSimpleStdString())
{
    m_server = std::make_unique<nx::network::socks5::Server>(
        []
        {
            return std::make_unique<VmsServerConnector>();
        });

    m_server->bind(network::SocketAddress::anyPrivateAddress);
    m_server->listen();
}

SocketAddress LocalProxyServer::address() const
{
    return m_server->address();
}

std::string LocalProxyServer::password() const
{
    return m_password;
}

LocalProxyServer::~LocalProxyServer()
{
    m_server->pleaseStopSync();
}

} // namespace nx::vms::client::desktop
