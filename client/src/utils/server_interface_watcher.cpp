#include "server_interface_watcher.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <utils/network/router.h>

QnServerInterfaceWatcher::QnServerInterfaceWatcher(QnRouter *router, QObject *parent) :
    QObject(parent)
{
    connect(router,     &QnRouter::connectionAdded,     this,   &QnServerInterfaceWatcher::at_connectionChanged, Qt::QueuedConnection);
    connect(router,     &QnRouter::connectionRemoved,   this,   &QnServerInterfaceWatcher::at_connectionChanged, Qt::QueuedConnection);
}

void QnServerInterfaceWatcher::at_connectionChanged(const QUuid &discovererId, const QUuid &peerId) {
    Q_UNUSED(discovererId);

    QnRouter *router = QnRouter::instance();
    if (!router)
        return;

    QnRoute route = QnRouter::instance()->routeTo(peerId);
    if (!route.isValid())
        return;

    QnMediaServerResourcePtr server = qnResPool->getResourceById(peerId).dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    server->setPrimaryIF(route.points.last().host);
}
