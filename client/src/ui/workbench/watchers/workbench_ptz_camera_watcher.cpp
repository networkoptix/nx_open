#include "workbench_ptz_camera_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

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

QList<QnVirtualCameraResourcePtr> QnWorkbenchPtzCameraWatcher::ptzCameras() const {
    return m_ptzCameras.toList();
}

void QnWorkbenchPtzCameraWatcher::addPtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_ptzCameras.contains(camera))
        return;

    m_ptzCameras.insert(camera);
    emit ptzCameraAdded(camera);
}

void QnWorkbenchPtzCameraWatcher::removePtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_ptzCameras.remove(camera))
        emit ptzCameraRemoved(camera);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzCameraWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    connect(camera.data(), SIGNAL(cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)), this, SLOT(at_resource_cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)));
    at_resource_cameraCapabilitiesChanged(camera);
}

void QnWorkbenchPtzCameraWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    disconnect(camera.data(), NULL, this, NULL);
    removePtzCamera(camera);
}

void QnWorkbenchPtzCameraWatcher::at_resource_cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        if(camera->getCameraCapabilities() & Qn::ContinuousPtzCapability) {
            addPtzCamera(camera);
        } else {
            removePtzCamera(camera);
        }
    }
}
