#include "client_ptz_controller_pool.h"

#include <core/ptz/remote_ptz_controller.h>
#include <core/ptz/caching_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_core_camera.h>

#include <core/resource_management/resource_pool.h>

QnClientPtzControllerPool::QnClientPtzControllerPool(QObject* parent):
    base_type(parent)
{
    // Controller may potentially be created with delay.
    connect(this, &QnPtzControllerPool::controllerChanged,
        this, &QnClientPtzControllerPool::cacheCameraPresets);
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

    connect(camera, &QnResource::propertyChanged, this,
        [this](const QnResourcePtr& resource, const QString& key)
        {
            const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            if (!NX_ASSERT(camera))
                return;

            if (key == ResourcePropertyKey::kUserPreferredPtzPresetType
                || key == ResourcePropertyKey::kDefaultPreferredPtzPresetType)
            {
                cacheCameraPresets(camera);
            }
        });

    connect(camera, &QnResource::statusChanged,
        this, &QnClientPtzControllerPool::cacheCameraPresets);

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
    if (!camera || !camera->isPtzSupported() || !camera->isOnline())
        return;

    auto controller = this->controller(camera);

    /* Controller may potentially be created with delay. */
    if (!controller)
        return;

    /* These presets will be cached in the QnCachingPtzController instance */
    QnPtzPresetList presets;
    controller->getPresets(&presets);
}
