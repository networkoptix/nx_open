#include "reconnect_helper.h"

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <nx/network/url/url_builder.h>
#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::core {

ReconnectHelper::ReconnectHelper(bool stickyReconnect, QObject* parent):
    base_type(parent)
{
    m_userName = commonModule()->currentUrl().userName();
    m_password = commonModule()->currentUrl().password();

    const auto currentServer = commonModule()->currentServer();
    if (currentServer && stickyReconnect)
        m_servers.append(currentServer);
    else  // List of all known servers. Should not be updated as we are disconnected.
        m_servers.append(resourcePool()->getAllServers(Qn::AnyStatus));

    // Check if there are no servers in the system.
    NX_ASSERT(!m_servers.empty());
    if (m_servers.empty())
        return;

    std::sort(m_servers.begin(), m_servers.end(),
        [](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            return left->getId() < right->getId();
        });

    m_currentIndex = std::max(m_servers.indexOf(currentServer), 0);
    const auto discoverManager = commonModule()->moduleDiscoveryManager();
}

QnMediaServerResourceList ReconnectHelper::servers() const
{
    return m_servers;
}

QnMediaServerResourcePtr ReconnectHelper::currentServer() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_servers.size())
        return QnMediaServerResourcePtr();

    return m_servers[m_currentIndex];
}

nx::utils::Url ReconnectHelper::currentUrl() const
{
    const auto server = currentServer();
    if (!server)
        return nx::utils::Url();

    const auto discoverManager = commonModule()->moduleDiscoveryManager();
    if (const auto endpoint = discoverManager->getEndpoint(server->getId()))
    {
        nx::network::url::Builder builder;
        builder.setEndpoint(*endpoint);
        builder.setUserName(m_userName);
        builder.setPassword(m_password);
        return builder.toUrl();
    }

    return nx::utils::Url();
}

void ReconnectHelper::next()
{
    if (m_servers.empty())
        return;

    m_currentIndex = (m_currentIndex + 1) % m_servers.size();
}

void ReconnectHelper::markServerAsInvalid(const QnMediaServerResourcePtr& server)
{
    m_servers.removeAll(server);
}

} // namespace nx::vms::client::core
