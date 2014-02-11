#include "client_ptz_controller_pool.h"

#include <core/ptz/remote_ptz_controller.h>
#include <core/ptz/caching_ptz_controller.h>
#include <core/resource/camera_resource.h>

void QnClientPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    base_type::registerResource(resource);
}

void QnClientPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    base_type::unregisterResource(resource);
}

QnPtzControllerPtr QnClientPtzControllerPool::createController(const QnResourcePtr &resource) const {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QnPtzControllerPtr();

    QnPtzControllerPtr controller;
    controller.reset(new QnRemotePtzController(camera));
    controller.reset(new QnCachingPtzController(controller));
    return controller;
}
