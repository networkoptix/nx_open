// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reconnect_helper.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::core {

ReconnectHelper::ReconnectHelper(SystemContext* systemContext, bool stickyReconnect):
    SystemContextAware(systemContext),
    m_servers(systemContext->resourcePool()->servers()),
    m_originalServerId(systemContext->currentServerId())
{
    auto currentServer = resourcePool()->getResourceById<QnMediaServerResource>(m_originalServerId);
    if (stickyReconnect && currentServer)
        m_servers = {currentServer};

    // Check if there are no servers in the system (e.g connection was broken before resources are
    // received).
    if (m_servers.empty())
        return;

    std::sort(m_servers.begin(), m_servers.end(),
        [](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            return left->getId() < right->getId();
        });

    m_currentIndex = std::max(m_servers.indexOf(currentServer), (qsizetype) 0);
}

QnMediaServerResourcePtr ReconnectHelper::currentServer() const
{
    // Reconnect process was not initialized.
    if (m_currentIndex < 0)
        return {};

    if (NX_ASSERT(m_currentIndex >= 0 && m_currentIndex < m_servers.size()))
        return m_servers[m_currentIndex];

    return {};
}

std::optional<nx::network::SocketAddress> ReconnectHelper::currentAddress() const
{
    const auto server = currentServer();
    if (!server)
        return std::nullopt;

    // Skip current server since connection will be restored automatically.
    // Prevents racing when connecting to several m_servers simultaneously.
    if (server->getId() == m_originalServerId)
        return std::nullopt;

    const auto discoverManager = appContext()->moduleDiscoveryManager();
    return discoverManager->getEndpoint(server->getId());
}

void ReconnectHelper::markCurrentServerAsInvalid()
{
    if (auto server = currentServer())
        m_servers.removeAll(server);
}

void ReconnectHelper::markOriginalServerAsInvalid()
{
    auto it = std::remove_if(m_servers.begin(), m_servers.end(),
        [this](const auto& server) { return server->getId() == m_originalServerId; });
    m_servers.erase(it, m_servers.end());
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
