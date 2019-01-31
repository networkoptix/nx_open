#include "client_ptz_controller_pool.h"

#include <core/ptz/remote_ptz_controller.h>
#include <core/ptz/caching_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_core_camera.h>

#include <core/resource_management/resource_pool.h>

QnClientPtzControllerPool::QnClientPtzControllerPool(QObject *parent /*= NULL*/)
    : base_type(parent)
{
    /* Auto-update presets when camera goes online. */
    connect(resourcePool(), &QnResourcePool::statusChanged, this, [this](const QnResourcePtr &resource)
	    {
	        cacheCameraPresets(resource.dynamicCast<QnVirtualCameraResource>());
	    });


    /* Controller may potentially be created with delay. */
    connect(this, &QnPtzControllerPool::controllerChanged, this,
        &QnClientPtzControllerPool::cacheCameraPresets);
}

void QnClientPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    base_type::registerResource(resource);

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera, &QnVirtualCameraResource::ptzCapabilitiesChanged, this,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            updateController(camera);
            cacheCameraPresets(camera);

            if (auto controller = this->controller(camera))
                emit controller->changed(Qn::CapabilitiesPtzField);

        });

    cacheCameraPresets(camera);
}

void QnClientPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    base_type::unregisterResource(resource);
}

QnPtzControllerPtr QnClientPtzControllerPool::createController(const QnResourcePtr &resource) const
{
    const auto camera = resource.dynamicCast<nx::vms::client::core::Camera>();
    if (!camera || !camera->isPtzSupported())
        return {};

    QnPtzControllerPtr controller;
    controller.reset(new QnRemotePtzController(camera));
    controller.reset(new QnCachingPtzController(controller));
    return controller;
}

void QnClientPtzControllerPool::cacheCameraPresets(const QnResourcePtr &resource)
{
    const auto camera = resource.dynamicCast<nx::vms::client::core::Camera>();
    if (!camera || !camera->isPtzSupported())
        return;

    if (camera->getStatus() != Qn::Online)
        return;

    auto controller = this->controller(camera);

    /* Controller may potentially be created with delay. */
    if (!controller)
        return;

    /* These presets will be cached in the QnCachingPtzController instance */
    QnPtzPresetList presets;
    controller->getPresets(&presets);
}
