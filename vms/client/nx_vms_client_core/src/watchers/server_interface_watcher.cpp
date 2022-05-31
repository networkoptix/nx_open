// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_interface_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_connection.h>

using namespace nx::vms::client::core;

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QObject* parent): QObject(parent)
{
    appContext()->moduleDiscoveryManager()->onSignals(this,
        &QnServerInterfaceWatcher::at_connectionChanged,
        &QnServerInterfaceWatcher::at_connectionChanged,
        &QnServerInterfaceWatcher::at_connectionChangedById);

    connect(resourcePool(),
        &QnResourcePool::statusChanged,
        this,
        &QnServerInterfaceWatcher::at_resourcePool_statusChanged);
    connect(resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &QnServerInterfaceWatcher::at_resourcePool_resourceAdded);
}

void QnServerInterfaceWatcher::at_connectionChanged(nx::vms::discovery::ModuleEndpoint module)
{
    at_connectionChangedById(module.id);
}

void QnServerInterfaceWatcher::at_connectionChangedById(QnUuid id)
{
    if (const auto server = resourcePool()->getResourceById<QnMediaServerResource>(id))
        updatePrimaryInterface(server);
}

void QnServerInterfaceWatcher::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::server))
        return;

    auto currentServer = this->currentServer();

    if (resource != currentServer)
        return;

    if (!NX_ASSERT(connection(), "Connection must be established"))
        return;

    const auto address = connectionAddress();
    currentServer->setPrimaryAddress(address);
    NX_DEBUG(this,
        "Initial set primary address of %1 (%2) to default %3",
        currentServer->getName(),
        currentServer->getId().toString(),
        address.toString());
    updatePrimaryInterface(currentServer);
}

void QnServerInterfaceWatcher::at_resourcePool_statusChanged(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    updatePrimaryInterface(server);
}

void QnServerInterfaceWatcher::updatePrimaryInterface(const QnMediaServerResourcePtr& server)
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
