#include "server_interface_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/router.h>

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QnRouter *router, QObject *parent) :
    QObject(parent)
{
    connect(router,     &QnRouter::connectionAdded,     this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(router,     &QnRouter::connectionRemoved,   this,   &QnServerInterfaceWatcher::at_connectionChanged);
    connect(qnResPool,  &QnResourcePool::statusChanged, this,   &QnServerInterfaceWatcher::at_resourcePool_statusChanged);
}

void QnServerInterfaceWatcher::at_connectionChanged(const QnUuid &discovererId, const QnUuid &peerId) {
    Q_UNUSED(discovererId);

    QnRouter *router = QnRouter::instance();
    if (!router)
        return;

    QnMediaServerResourcePtr server = qnResPool->getResourceById(peerId).dynamicCast<QnMediaServerResource>();

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
    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (!route.isValid())
        return;

    server->setPrimaryIF(route.points.last().host);
}
