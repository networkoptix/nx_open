// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "reconnect_helper.h"

#include <client_core/client_core_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/discovery/manager.h>

namespace nx::vms::client::core {

ReconnectHelper::ReconnectHelper(std::optional<QnUuid> stickyReconnectTo)
{
    auto currentServer = resourcePool()->getResourceById<QnMediaServerResource>(
        stickyReconnectTo.value_or(QnUuid()));

    if (currentServer && stickyReconnectTo.has_value())
        m_servers.append(currentServer);
    else  // List of all known m_servers. Should not be updated as we are disconnected.
        m_servers.append(resourcePool()->servers());

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
    if (server->getId() == qnClientCoreModule->networkModule()->currentServerId())
        return std::nullopt;

    const auto discoverManager = appContext()->moduleDiscoveryManager();
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
