#include "ptz_controller_pool.h"

#include <common/common_module.h>

// TODO: #Elric managment? rename to managEment
#include <core/resource_managment/resource_pool.h>
#include <core/resource_managment/resource_data_pool.h>
#include <core/resource/camera_resource.h>

#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/relative_ptz_controller.h>

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

	PtzData data;
    data.deviceController.reset(camera->createPtzController());
    if(!data.deviceController)
        return;

	if(data.deviceController) {
        if(data.deviceController->hasCapabilities(Qn::LogicalPositionSpaceCapability)) {
            data.logicalController = data.deviceController;
        } else if(QnPtzMapperPtr mapper = qnCommon->dataPool()->data(camera).ptzMapper()) {
            data.logicalController.reset(new QnMappedPtzController(mapper, data.deviceController));
        }
    }

	if(data.logicalController) {
        if(data.logicalController->hasCapabilities(Qn::ScreenSpaceMovementCapability)) {
            data.relativeController = data.logicalController;
        } else {
            data.relativeController.reset(new QnRelativePtzController(data.logicalController));
        }
    }

	m_dataByResource.insert(resource, data);


}
