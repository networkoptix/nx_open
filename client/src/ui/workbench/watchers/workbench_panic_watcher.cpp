#include "workbench_panic_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

QnWorkbenchPanicWatcher::QnWorkbenchPanicWatcher(QObject *parent):
    base_type(parent),
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
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    connect(server, &QnMediaServerResource::panicModeChanged, this, &QnWorkbenchPanicWatcher::at_resource_panicModeChanged);

    m_servers.insert(server);
    if(server->getPanicMode() != Qn::PM_None)
        m_panicServers.insert(server);

    updatePanicMode();
}

void QnWorkbenchPanicWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    m_servers.remove(server);
    m_panicServers.remove(server);

    disconnect(server.data(), NULL, this, NULL);

    updatePanicMode();
}

void QnWorkbenchPanicWatcher::at_resource_panicModeChanged(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    if(server->getPanicMode() != Qn::PM_None) {
        m_panicServers.insert(server);
    } else {
        m_panicServers.remove(server);
    }

    updatePanicMode();
}

