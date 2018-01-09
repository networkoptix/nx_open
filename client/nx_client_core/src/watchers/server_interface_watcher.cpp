#include "server_interface_watcher.h"

#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QObject *parent) :
    QObject(parent)
{
    commonModule()->moduleDiscoveryManager()->onSignals(this,
        &QnServerInterfaceWatcher::at_connectionChanged,
        &QnServerInterfaceWatcher::at_connectionChanged,
        &QnServerInterfaceWatcher::at_connectionChangedById);

    connect(resourcePool(), &QnResourcePool::statusChanged,
        this, &QnServerInterfaceWatcher::at_resourcePool_statusChanged);
    connect(resourcePool(), &QnResourcePool::resourceAdded,
        this, &QnServerInterfaceWatcher::at_resourcePool_resourceAdded);
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

void QnServerInterfaceWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr currentServer = commonModule()->currentServer();
    if (resource != currentServer)
        return;

    const nx::network::SocketAddress address = 
        nx::network::url::getEndpoint(commonModule()->currentUrl());
    currentServer->setPrimaryAddress(address);
    NX_LOG(
        lit("QnServerInterfaceWatcher: Set primary address of %1 (%2) to default %3")
            .arg(currentServer->getName())
            .arg(currentServer->getId().toString())
            .arg(address.toString()),
        cl_logDEBUG1);
}

void QnServerInterfaceWatcher::at_resourcePool_statusChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    updatePrimaryInterface(server);
}

void QnServerInterfaceWatcher::updatePrimaryInterface(const QnMediaServerResourcePtr &server)
{
    const auto serverId = server->getId();
    const auto module = commonModule()->moduleDiscoveryManager()->getModule(serverId);
    if (!module)
        return;

    if (module->sslAllowed != server->isSslAllowed())
        server->setSslAllowed(module->sslAllowed);

    if (module->endpoint != server->getPrimaryAddress())
    {
        server->setPrimaryAddress(module->endpoint);
        NX_LOG(lm("QnServerInterfaceWatcher: Set primary address of %1 (%2) to %3")
            .args(server->getName(), server->getId(), module->endpoint), cl_logDEBUG1);
    }
}
