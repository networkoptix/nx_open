#include "server_interface_watcher.h"

#include <api/app_server_connection.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <network/module_finder.h>
#include <nx/utils/log/log.h>

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QObject *parent) :
    QObject(parent)
{
    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleAddressFound,     this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleAddressLost,     this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged, this,   &QnServerInterfaceWatcher::at_resourcePool_statusChanged);
    connect(qnResPool,  &QnResourcePool::resourceAdded, this,   &QnServerInterfaceWatcher::at_resourcePool_resourceAdded);
}

void QnServerInterfaceWatcher::at_connectionChanged(const QnModuleInformation &moduleInformation) 
{
    QnMediaServerResourcePtr server = qnResPool->getResourceById<QnMediaServerResource>(moduleInformation.id);
    if (!server)
        return;
    updatePriaryInterface(server);
}

void QnServerInterfaceWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr currentServer = qnCommon->currentServer();
    if (resource != currentServer)
        return;

    const SocketAddress address(QnAppServerConnectionFactory::url());
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

    updatePriaryInterface(server);
}

void QnServerInterfaceWatcher::updatePriaryInterface(const QnMediaServerResourcePtr &server)
{
    const auto newAddress = QnModuleFinder::instance()->primaryAddress(server->getId());
    if (!newAddress.isNull() && newAddress != server->getPrimaryAddress())
    {
        server->setPrimaryAddress(newAddress);
        NX_LOG(lit("QnServerInterfaceWatcher: Set primary address of %1 (%2) to %3")
            .arg(server->getName()).arg(server->getId().toString()).arg(newAddress.toString()),
            cl_logDEBUG1);
    }
}
