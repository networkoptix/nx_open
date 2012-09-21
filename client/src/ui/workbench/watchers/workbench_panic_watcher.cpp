#include "workbench_panic_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

QnWorkbenchPanicWatcher::QnWorkbenchPanicWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_panicMode(false)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchPanicWatcher::~QnWorkbenchPanicWatcher() {
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    disconnect(resourcePool(), NULL, this, NULL);
}

void QnWorkbenchPanicWatcher::updatePanicMode() {
    bool panicMode = !m_servers.isEmpty() && m_panicServers.size() == m_servers.size();
    if(m_panicMode == panicMode)
        return;

    m_panicMode = panicMode;

    emit panicModeChanged();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPanicWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>();
    if(!server)
        return;

    connect(server.data(), SIGNAL(panicModeChanged(const QnVideoServerResourcePtr &)), this, SLOT(at_resource_panicModeChanged(const QnVideoServerResourcePtr &)));

    m_servers.insert(server);
    if(server->isPanicMode())
        m_panicServers.insert(server);

    updatePanicMode();
}

void QnWorkbenchPanicWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnVideoServerResourcePtr server = resource.dynamicCast<QnVideoServerResource>();
    if(!server)
        return;

    m_servers.remove(server);
    m_panicServers.remove(server);

    disconnect(server.data(), NULL, this, NULL);

    updatePanicMode();
}

void QnWorkbenchPanicWatcher::at_resource_panicModeChanged(const QnVideoServerResourcePtr &resource) {
    if(resource->isPanicMode()) {
        m_panicServers.insert(resource);
    } else {
        m_panicServers.remove(resource);
    }

    updatePanicMode();
}

