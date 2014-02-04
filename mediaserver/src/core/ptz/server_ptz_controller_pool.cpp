#include "server_ptz_controller_pool.h"

#include <common/common_module.h>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_data_pool.h>
#include <core/resource/camera_resource.h>

#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/workaround_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>


void QnServerPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    connect(resource, &QnResource::initialized, this, &QnServerPtzControllerPool::updateController, Qt::QueuedConnection);
    base_type::registerResource(resource);
}

void QnServerPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    base_type::unregisterResource(resource);
    disconnect(resource, NULL, this, NULL);
}

QnPtzControllerPtr QnServerPtzControllerPool::createController(const QnResourcePtr &resource) {
    if(!resource->isInitialized())
        return QnPtzControllerPtr();

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QnPtzControllerPtr();

    QnPtzControllerPtr controller(camera->createPtzController());
    if(!controller)
        return QnPtzControllerPtr();

    if(QnMappedPtzController::extends(controller->getCapabilities()))
        if(QnPtzMapperPtr mapper = qnCommon->dataPool()->data(camera).ptzMapper())
            controller.reset(new QnMappedPtzController(mapper, controller));

    if(QnViewportPtzController::extends(controller->getCapabilities()))
        controller.reset(new QnViewportPtzController(controller));

    if(QnPresetPtzController::extends(controller->getCapabilities()))
        controller.reset(new QnPresetPtzController(controller));

    if(QnTourPtzController::extends(controller->getCapabilities()))
        controller.reset(new QnTourPtzController(controller));

    if(QnWorkaroundPtzController::extends(controller->getCapabilities()))
        controller.reset(new QnWorkaroundPtzController(controller));

    camera->setPtzCapabilities(controller->getCapabilities());
    QnAppServerConnectionFactory::createConnection2Sync()->getCameraManager()->addCamera(camera, this, &QnServerPtzControllerPool::at_addCameraDone);
    
    return controller;
}

void QnServerPtzControllerPool::at_addCameraDone(ec2::ErrorCode, const QnVirtualCameraResourceList &)
{

}
