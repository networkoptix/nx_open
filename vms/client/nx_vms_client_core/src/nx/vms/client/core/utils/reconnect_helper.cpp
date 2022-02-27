// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reconnect_helper.h"

#include <common/common_module.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::core {

ReconnectHelper::ReconnectHelper(bool stickyReconnect)
{
    const auto currentServer = commonModule()->currentServer();
    if (currentServer && stickyReconnect)
        m_servers.append(currentServer);
    else  // List of all known m_servers. Should not be updated as we are disconnected.
        m_servers.append(resourcePool()->servers());

    // Check if there are no m_servers in the system.
    NX_ASSERT(!m_servers.empty());
    if (m_servers.empty())
        return;

    std::sort(m_servers.begin(), m_servers.end(),
        [](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            return left->getId() < right->getId();
        });

    m_currentIndex = std::max(m_servers.indexOf(currentServer), 0);
}

QnMediaServerResourcePtr ReconnectHelper::currentServer() const
{
    if (NX_ASSERT(m_currentIndex >= 0 && m_currentIndex < m_servers.size()))
        return m_servers[m_currentIndex];

    return QnMediaServerResourcePtr();
}

std::optional<nx::network::SocketAddress> ReconnectHelper::currentAddress() const
{
    const auto server = currentServer();
    if (!server)
        return std::nullopt;

    // Skip current server since connection will be restored automatically.
    // Prevents racing when connecting to several m_servers simultaneously.
    if (server->getId() == commonModule()->remoteGUID())
        return std::nullopt;

    const auto discoverManager = commonModule()->moduleDiscoveryManager();
    return discoverManager->getEndpoint(server->getId());
}

void ReconnectHelper::markCurrentServerAsInvalid()
{
    if (auto server = currentServer())
        m_servers.removeAll(server);
}

void ReconnectHelper::next()
{
    if (m_servers.empty())
        return;

    m_currentIndex = (m_currentIndex + 1) % m_servers.size();
}

bool ReconnectHelper::empty() const
{
    return m_servers.empty();
}

} // namespace nx::vms::client::core
