#include "workbench_ptz_cameras_watcher.h"

#include <utils/common/checked_cast.h>

#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <client/client_meta_types.h>

QnWorkbenchPtzCamerasWatcher::QnWorkbenchPtzCamerasWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    QnClientMetaTypes::initialize();

    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchPtzCamerasWatcher::~QnWorkbenchPtzCamerasWatcher() {
    while(!m_ptzCameras.isEmpty())
        at_resourcePool_resourceRemoved(*m_ptzCameras.begin());
}

QList<QnVirtualCameraResourcePtr> QnWorkbenchPtzCamerasWatcher::ptzCameras() const {
    return m_ptzCameras.toList();
}

void QnWorkbenchPtzCamerasWatcher::addPtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_ptzCameras.contains(camera))
        return;

    m_ptzCameras.insert(camera);
    emit ptzCameraAdded(camera);
}

void QnWorkbenchPtzCamerasWatcher::removePtzCamera(const QnVirtualCameraResourcePtr &camera) {
    if(m_ptzCameras.remove(camera))
        emit ptzCameraRemoved(camera);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzCamerasWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    connect(camera.data(), SIGNAL(cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)), this, SLOT(at_cameraResource_cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)));
    at_cameraResource_cameraCapabilitiesChanged(camera);
}

void QnWorkbenchPtzCamerasWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    disconnect(camera.data(), NULL, this, NULL);
    removePtzCamera(camera);
}

void QnWorkbenchPtzCamerasWatcher::at_cameraResource_cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>()) {
        if(camera->getCameraCapabilities() & QnSecurityCamResource::ContinuousPtzCapability) {
            addPtzCamera(camera);
        } else {
            removePtzCamera(camera);
        }
    }
}
