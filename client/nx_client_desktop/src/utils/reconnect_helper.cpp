#include "reconnect_helper.h"

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <nx/network/url/url_builder.h>

#include <nx/utils/string.h>
#include <nx/vms/discovery/manager.h>

QnReconnectHelper::QnReconnectHelper(QObject* parent):
    QObject(parent)
{
    m_userName = commonModule()->currentUrl().userName();
    m_password = commonModule()->currentUrl().password();

    const auto currentServer = commonModule()->currentServer();
    if (currentServer && qnSettings->stickReconnectToServer())
        m_servers.append(currentServer);
    else  // List of all known servers. Should not be updated as we are disconnected.
        m_servers.append(resourcePool()->getAllServers(Qn::AnyStatus));

    // Check if there are no servers in the system.
    NX_ASSERT(!m_servers.empty());
    if (m_servers.empty())
        return;

    auto serverName =
        [](const QnMediaServerResourcePtr& server)
        {
            return QnResourceDisplayInfo(server).toString(qnSettings->extraInfoInTree());
        };

    std::sort(m_servers.begin(), m_servers.end(),
        [serverName](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            return nx::utils::naturalStringLess(serverName(left), serverName(right));
        });

    m_currentIndex = std::max(m_servers.indexOf(currentServer), 0);
    const auto discoverManager = commonModule()->moduleDiscoveryManager();
}

QnMediaServerResourceList QnReconnectHelper::servers() const
{
    return m_servers;
}

QnMediaServerResourcePtr QnReconnectHelper::currentServer() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size())
        return QnMediaServerResourcePtr();

    return m_servers[m_currentIndex];
}

QUrl QnReconnectHelper::currentUrl() const
{
    const auto server = currentServer();
    if (!server)
        return QUrl();

    const auto discoverManager = commonModule()->moduleDiscoveryManager();
    if (const auto endpoint = discoverManager->getEndpoint(server->getId()))
    {
        nx::network::url::Builder builder;
        builder.setEndpoint(*endpoint);
        builder.setUserName(m_userName);
        builder.setPassword(m_password);
        return builder.toUrl();
    }

    return QUrl();
}

void QnReconnectHelper::next()
{
    if (m_servers.empty())
        return;

    m_currentIndex = (m_currentIndex + 1) % m_servers.size();
}

void QnReconnectHelper::markServerAsInvalid(const QnMediaServerResourcePtr& server)
{
    m_servers.removeAll(server);
}

