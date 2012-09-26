#include "workbench_schedule_watcher.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

QnWorkbenchScheduleWatcher::QnWorkbenchScheduleWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_scheduleEnabled(false)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));

    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchScheduleWatcher::~QnWorkbenchScheduleWatcher() {
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceRemoved(resource);

    disconnect(resourcePool(), NULL, this, NULL);
}

bool QnWorkbenchScheduleWatcher::isScheduleEnabled() const {
    return m_scheduleEnabled;
}

void QnWorkbenchScheduleWatcher::updateScheduleEnabled() {
    bool scheduleEnabled = !m_scheduleEnabledCameras.empty();
    if(m_scheduleEnabled == scheduleEnabled)
        return;

    m_scheduleEnabled = scheduleEnabled;
    emit scheduleEnabledChanged();
}

void QnWorkbenchScheduleWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    connect(camera.data(), SIGNAL(scheduleDisabledChanged(const QnVirtualCameraResourcePtr &)), this, SLOT(at_resource_scheduleDisabledChanged(const QnVirtualCameraResourcePtr &)));

    if(!camera->isScheduleDisabled())
        m_scheduleEnabledCameras.insert(camera);

    updateScheduleEnabled();
}

void QnWorkbenchScheduleWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    m_scheduleEnabledCameras.remove(camera);

    disconnect(camera.data(), NULL, this, NULL);

    updateScheduleEnabled();
}

void QnWorkbenchScheduleWatcher::at_resource_scheduleDisabledChanged(const QnVirtualCameraResourcePtr &resource) {
    if(resource->isScheduleDisabled()) {
        m_scheduleEnabledCameras.remove(resource);
    } else {
        m_scheduleEnabledCameras.insert(resource);
    }

    updateScheduleEnabled();
}
