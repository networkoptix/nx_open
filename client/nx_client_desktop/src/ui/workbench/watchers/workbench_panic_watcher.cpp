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

    resourcePool()->disconnect(this);
}

void QnWorkbenchPanicWatcher::updatePanicMode() {
    auto servers = resourcePool()->getResources<QnMediaServerResource>();
    bool panicMode = std::any_of(servers.cbegin(), servers.cend(), [](const QnMediaServerResourcePtr &server) {
        return server->getPanicMode() != Qn::PM_None && server->getStatus() == Qn::Online;
    });

    if(m_panicMode == panicMode)
        return;

    m_panicMode = panicMode;

    emit panicModeChanged(panicMode);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPanicWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    connect(server, &QnMediaServerResource::statusChanged,    this, &QnWorkbenchPanicWatcher::updatePanicMode);
    connect(server, &QnMediaServerResource::propertyChanged,  this, &QnWorkbenchPanicWatcher::updatePanicMode);
    updatePanicMode();
}

void QnWorkbenchPanicWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    const auto server = resource.dynamicCast<QnMediaServerResource>();
    if (!server)
        return;

    server->disconnect(this);
    updatePanicMode();
}
