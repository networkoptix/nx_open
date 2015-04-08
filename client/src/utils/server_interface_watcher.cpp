#include "server_interface_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include "utils/network/module_finder.h"

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QObject *parent) :
    QObject(parent)
{
    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleAddressFound,     this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(QnModuleFinder::instance(),     &QnModuleFinder::moduleAddressLost,     this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged, this,   &QnServerInterfaceWatcher::at_resourcePool_statusChanged);
}

void QnServerInterfaceWatcher::at_connectionChanged(const QnModuleInformation &moduleInformation) 
{
    QnMediaServerResourcePtr server = qnResPool->getResourceById(moduleInformation.id).dynamicCast<QnMediaServerResource>();
    if (!server)
        return;
    updatePriaryInterface(server);
}

void QnServerInterfaceWatcher::at_resourcePool_statusChanged(const QnResourcePtr &resource) {
    if (!resource->hasFlags(Qn::server))
        return;

    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    updatePriaryInterface(server);
}

void QnServerInterfaceWatcher::updatePriaryInterface(const QnMediaServerResourcePtr &server) {
    SocketAddress newAddr = QnModuleFinder::instance()->primaryAddress(server->getId());
    if (!newAddr.isNull() && newAddr.address.toString() != QUrl(server->getApiUrl()).host())
        server->setPrimaryAddress(newAddr);
}
