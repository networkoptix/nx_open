#include "workbench_ptz_camera_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

QnWorkbenchPtzCameraWatcher::QnWorkbenchPtzCameraWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchPtzCameraWatcher::~QnWorkbenchPtzCameraWatcher() {
    while(!m_ptzCameras.isEmpty())
        at_resourcePool_resourceRemoved(*m_ptzCameras.begin());
}

const QSet<QnVirtualCameraResourcePtr> &QnWorkbenchPtzCameraWatcher::ptzCameras() const {
    return m_ptzCameras;
}

void QnWorkbenchPtzCameraWatcher::addPtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_ptzCameras.contains(camera))
        return;

    m_ptzCameras.insert(camera);
    emit ptzCameraAdded(camera);
}

void QnWorkbenchPtzCameraWatcher::removePtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if (m_ptzCameras.remove(camera))
        emit ptzCameraRemoved(camera);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera.data(), SIGNAL(ptzCapabilitiesChanged(const QnResourcePtr &)), this, SLOT(at_resource_ptzCapabilitiesChanged(const QnResourcePtr &)));
    at_resource_ptzCapabilitiesChanged(camera);
}

void QnWorkbenchPtzCameraWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    disconnect(resource.data(), NULL, this, NULL);
    removePtzCamera(camera);
}

void QnWorkbenchPtzCameraWatcher::at_resource_ptzCapabilitiesChanged(const QnResourcePtr &resource)
{
    if (QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (resource->getPtzCapabilities())
            addPtzCamera(camera);
        else
            removePtzCamera(camera);
    }
}
