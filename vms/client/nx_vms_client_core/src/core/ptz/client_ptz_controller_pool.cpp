// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_ptz_controller_pool.h"

#include <core/ptz/caching_ptz_controller.h>
#include <core/ptz/remote_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_core_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/delete_later.h>

QnClientPtzControllerPool::QnClientPtzControllerPool(
    nx::vms::common::SystemContext* systemContext,
    QObject* parent)
    :
    base_type(systemContext, parent)
{
    // Controller may potentially be created with delay.
    connect(this, &QnPtzControllerPool::controllerChanged,
        this, &QnClientPtzControllerPool::cacheCameraPresets);
    init();
}

void QnClientPtzControllerPool::registerResource(const QnResourcePtr &resource)
{
    base_type::registerResource(resource);

    if (auto server = resource.dynamicCast<QnMediaServerResource>();
        server && !server->hasFlags(Qn::fake))
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

    QnPtzControllerPtr baseController(new QnRemotePtzController(camera));
    QnPtzControllerPtr result(new QnCachingPtzController(baseController), &qnDeleteLater);
    return result;
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

    // Cache presets and tours in the QnCachingPtzController instance.
    QnPtzPresetList presets;
    controller->getPresets(&presets);

    QnPtzTourList tours;
    controller->getTours(&tours);
}

void QnClientPtzControllerPool::reinitServerCameras(const QnMediaServerResourcePtr& server)
{
    for (auto camera: server->resourcePool()->getAllCameras(server, /*ignoreDesktopCameras*/ true))
    {
        if (controller(camera))
            updateController(camera);
    }
}
