#include "server_ptz_controller_pool.h"

#include <common/common_module.h>

#include <api/app_server_connection.h>

// TODO: #Elric managment? rename to managEment
#include <core/resource_managment/resource_data_pool.h>
#include <core/resource/camera_resource.h>

#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/workaround_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>


void QnServerPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    connect(resource, &QnResource::initAsyncFinished, this, &QnPtzControllerPool::updateController);
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

    if(QnMappedPtzController::extends(controller))
        if(QnPtzMapperPtr mapper = qnCommon->dataPool()->data(camera).ptzMapper())
            controller.reset(new QnMappedPtzController(mapper, controller));

    if(QnViewportPtzController::extends(controller))
        controller.reset(new QnViewportPtzController(controller));

    if(QnPresetPtzController::extends(controller))
        controller.reset(new QnPresetPtzController(controller));

    if(QnTourPtzController::extends(controller))
        controller.reset(new QnTourPtzController(controller));

    controller.reset(new QnWorkaroundPtzController(controller));

    camera->setPtzCapabilities(controller->getCapabilities());
    QnAppServerConnectionFactory::createConnection()->saveAsync(camera, NULL, NULL);
    
    return controller;
}
