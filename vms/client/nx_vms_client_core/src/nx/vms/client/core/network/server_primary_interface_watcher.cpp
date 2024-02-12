// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_primary_interface_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>

namespace nx::vms::client::core {

ServerPrimaryInterfaceWatcher::ServerPrimaryInterfaceWatcher(
    SystemContext* systemContext,
    QObject* parent)
    :
    QObject(parent),
    SystemContextAware(systemContext)
{
    appContext()->moduleDiscoveryManager()->onSignals(this,
        &ServerPrimaryInterfaceWatcher::onConnectionChanged,
        &ServerPrimaryInterfaceWatcher::onConnectionChanged,
        &ServerPrimaryInterfaceWatcher::onConnectionChangedById);

    connect(resourcePool(),
        &QnResourcePool::statusChanged,
        this,
        &ServerPrimaryInterfaceWatcher::onResourcePoolStatusChanged);
    connect(resourcePool(),
        &QnResourcePool::resourcesAdded,
        this,
        &ServerPrimaryInterfaceWatcher::onResourcesAdded);
}

void ServerPrimaryInterfaceWatcher::onConnectionChanged(nx::vms::discovery::ModuleEndpoint module)
{
    onConnectionChangedById(module.id);
}

void ServerPrimaryInterfaceWatcher::onConnectionChangedById(nx::Uuid id)
{
    if (const auto server = resourcePool()->getResourceById<QnMediaServerResource>(id))
        updatePrimaryInterface(server);
}

void ServerPrimaryInterfaceWatcher::onResourcesAdded(const QnResourceList& resources)
{
    auto currentServerId = this->currentServerId();
    if (currentServerId.isNull())
        return;

    auto connection = this->connection();
    if (!NX_ASSERT(connection, "Connection must already be established"))
        return;

    const auto address = connection->address();

    for (const auto& resource: resources)
    {
        if (resource->getId() != currentServerId)
            continue;

        auto currentServer = resource.dynamicCast<QnMediaServerResource>();
        if (!NX_ASSERT(currentServer))
            return;

        currentServer->setPrimaryAddress(address);
        NX_DEBUG(this,
            "Initial set primary address of %1 (%2) to default %3",
            currentServer->getName(),
            currentServer->getId().toString(),
            address.toString());
        updatePrimaryInterface(currentServer);
        return;
    }
}

void ServerPrimaryInterfaceWatcher::onResourcePoolStatusChanged(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    updatePrimaryInterface(server);
}

void ServerPrimaryInterfaceWatcher::updatePrimaryInterface(const QnMediaServerResourcePtr& server)
{
    const auto serverId = server->getId();
    const auto module = appContext()->moduleDiscoveryManager()->getModule(serverId);
    if (!module)
        return;

    const auto address = module->endpoint;
    if (address != server->getPrimaryAddress())
    {
        server->setPrimaryAddress(module->endpoint);
        NX_DEBUG(this,
            "Update primary address of %1 (%2) to default %3",
            server->getName(),
            server->getId().toString(),
            address.toString());
    }
}

} // namespace nx::vms::client::core
