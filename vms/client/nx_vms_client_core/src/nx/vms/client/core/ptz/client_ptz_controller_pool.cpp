// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_ptz_controller_pool.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/resource/camera.h>
#include <utils/common/delete_later.h>

#include "caching_ptz_controller.h"
#include "remote_ptz_controller.h"

namespace nx::vms::client::core {
namespace ptz {

ControllerPool::ControllerPool(common::SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent)
{
    // Controller may potentially be created with delay.
    connect(this, &QnPtzControllerPool::controllerChanged,
        this, &ControllerPool::cacheCameraPresets);
    init();
}

void ControllerPool::registerResource(const QnResourcePtr &resource)
{
    base_type::registerResource(resource);

    if (auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        reinitServerCameras(server);
        return;
    }

    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera.get(), &QnVirtualCameraResource::ptzCapabilitiesChanged, this,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            updateController(camera);
            cacheCameraPresets(camera);

            if (auto controller = this->controller(camera))
                emit controller->changed(nx::vms::common::ptz::DataField::capabilities);
        });

    connect(camera.get(), &QnResource::propertyChanged, this,
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

    connect(camera.get(), &QnResource::statusChanged,
        this, &ControllerPool::cacheCameraPresets);

    cacheCameraPresets(camera);
}

void ControllerPool::unregisterResource(const QnResourcePtr &resource) {
    base_type::unregisterResource(resource);
}

QnPtzControllerPtr ControllerPool::createController(const QnResourcePtr &resource) const
{
    const auto camera = resource.dynamicCast<nx::vms::client::core::Camera>();
    if (!camera || !camera->isPtzSupported())
        return {};

    QnPtzControllerPtr baseController(new RemotePtzController(camera));
    QnPtzControllerPtr result(new CachingPtzController(baseController), &qnDeleteLater);
    return result;
}

void ControllerPool::cacheCameraPresets(const QnResourcePtr &resource)
{
    const auto camera = resource.dynamicCast<nx::vms::client::core::Camera>();
    if (!camera || !camera->isPtzSupported() || !camera->isOnline())
        return;

    auto controller = this->controller(camera);

    /* Controller may potentially be created with delay. */
    if (!controller)
        return;

    // Cache presets and tours in the CachingPtzController instance.
    QnPtzPresetList presets;
    controller->getPresets(&presets);

    QnPtzTourList tours;
    controller->getTours(&tours);
}

void ControllerPool::reinitServerCameras(const QnMediaServerResourcePtr& server)
{
    for (auto camera: server->resourcePool()->getAllCameras(server, /*ignoreDesktopCameras*/ true))
    {
        if (controller(camera))
            updateController(camera);
    }
}

} // namespace ptz
} // namespace nx::vms::client::core
