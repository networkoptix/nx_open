#include "workbench_user_layout_count_watcher.h"

#include <utils/common/checked_cast.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/workbench/workbench_context.h>

QnWorkbenchUserLayoutCountWatcher::QnWorkbenchUserLayoutCountWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_layoutCount(0)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    connect(context(),      SIGNAL(userChanged(const QnUserResourcePtr &)), this,   SLOT(at_context_userChanged()));

    at_context_userChanged();
}

QnWorkbenchUserLayoutCountWatcher::~QnWorkbenchUserLayoutCountWatcher() {
    return;
}

void QnWorkbenchUserLayoutCountWatcher::updateLayoutCount() {
    int layoutCount = m_layouts.size();
    if(m_layoutCount == layoutCount)
        return;

    m_layoutCount = layoutCount;

    emit layoutCountChanged();
}

void QnWorkbenchUserLayoutCountWatcher::at_context_userChanged() {
    m_currentUserId = context()->user() ? context()->user()->getId() : QUuid();

    m_layouts.clear();
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource, false);

    updateLayoutCount();
}

void QnWorkbenchUserLayoutCountWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource, bool update) {
    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(!layout)
        return;

    connect(layout.data(), SIGNAL(parentIdChanged(const QnResourcePtr &)), this, SLOT(at_resource_parentIdChanged(const QnResourcePtr &)));
    at_resource_parentIdChanged(layout, update);
}

void QnWorkbenchUserLayoutCountWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource, bool update) {
    QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
    if(!layout)
        return;

    disconnect(layout.data(), NULL, this, NULL);

    m_layouts.remove(layout);
    if(update)
        updateLayoutCount();
}

void QnWorkbenchUserLayoutCountWatcher::at_resource_parentIdChanged(const QnResourcePtr &resource, bool update) {
    if(resource->getParentId() == m_currentUserId) {
        m_layouts.insert(resource);
    } else {
        m_layouts.remove(resource);
    }
        
    if(update)
        updateLayoutCount();
}

