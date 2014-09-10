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
#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/home_ptz_controller.h>

QnServerPtzControllerPool::QnServerPtzControllerPool(QObject *parent): 
    base_type(parent) 
{
    setConstructionMode(ThreadedControllerConstruction);
}

void QnServerPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    // TODO: #Elric we're creating controller from main thread. 
    // Controller ctor may take some time (several seconds).
    // => main thread will stall.
    connect(resource, &QnResource::initializedChanged, this, &QnServerPtzControllerPool::updateController, Qt::QueuedConnection);
    base_type::registerResource(resource);
}

void QnServerPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    base_type::unregisterResource(resource);
    disconnect(resource, NULL, this, NULL);
}

QnPtzControllerPtr QnServerPtzControllerPool::createController(const QnResourcePtr &resource) const {
    if(resource->flags() & Qn::foreigner)
        return QnPtzControllerPtr(); /* That's not our resource! */

    if(!resource->isInitialized())
        return QnPtzControllerPtr();

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QnPtzControllerPtr();

    QnPtzControllerPtr controller(camera->createPtzController());
    if(controller) {
        if(QnMappedPtzController::extends(controller->getCapabilities()))
            if(QnPtzMapperPtr mapper = qnCommon->dataPool()->data(camera).value<QnPtzMapperPtr>(lit("ptzMapper")))
                controller.reset(new QnMappedPtzController(mapper, controller));

        if(QnViewportPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnViewportPtzController(controller));

        if(QnPresetPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnPresetPtzController(controller));

        if(QnTourPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnTourPtzController(controller));

        if(QnActivityPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnActivityPtzController(QnActivityPtzController::Server, controller));

        if(QnHomePtzController::extends(controller->getCapabilities()))
            controller.reset(new QnHomePtzController(controller));

        if(QnWorkaroundPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnWorkaroundPtzController(controller));
    }

    Qn::PtzCapabilities caps = controller ? controller->getCapabilities() : Qn::NoPtzCapabilities;
    if (camera->getPtzCapabilities() != caps) {
        camera->setPtzCapabilities(caps);
        camera->saveParamsAsync();
    }
    return controller;
}

void QnServerPtzControllerPool::at_addCameraDone(int, ec2::ErrorCode, const QnVirtualCameraResourceList &)
{

}
