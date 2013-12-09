#include "ptz_controller_pool.h"

#include <common/common_module.h>

#include <api/app_server_connection.h>

// TODO: #Elric managment? rename to managEment
#include <core/resource_managment/resource_pool.h>
#include <core/resource_managment/resource_data_pool.h>
#include <core/resource/camera_resource.h>

#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/workaround_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/tour_ptz_controller.h>


QnPtzControllerPool::QnPtzControllerPool(QObject *parent):
	base_type(parent)
{
	QnResourcePool *resourcePool = qnResPool;
    connect(resourcePool, SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnPtzControllerPool::~QnPtzControllerPool() {
	return;
}

QnPtzControllerPtr QnPtzControllerPool::controller(const QnResourcePtr &resource) const {
	auto pos = m_dataByResource.find(resource);
	if(pos == m_dataByResource.end())
		return QnPtzControllerPtr();

    return pos->controller;
}

void QnPtzControllerPool::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
	connect(resource, SIGNAL(initAsyncFinished(const QnResourcePtr &, bool)), this, SLOT(at_resource_initAsyncFinished(const QnResourcePtr &)));

	if(resource->isInitialized())
		at_resource_initAsyncFinished(resource);
}

void QnPtzControllerPool::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
	disconnect(resource, NULL, this, NULL);
}

void QnPtzControllerPool::at_resource_initAsyncFinished(const QnResourcePtr &resource) {
	if(!resource->isInitialized())
		return;

	QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
	if(!camera)
		return;

	QnPtzControllerPtr controller(camera->createPtzController());
    if(!controller)
        return;

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

    resource->setPtzCapabilities(controller->getCapabilities());
    QnAppServerConnectionFactory::createConnection()->saveAsync(camera, NULL, NULL);

    PtzData data;
    data.controller = controller;
	m_dataByResource.insert(resource, data);
}



