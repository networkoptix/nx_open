// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_advanced_parameters_manifest_manager.h"

#include <api/server_rest_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <nx/network/rest/params.h>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>

namespace nx::vms::client::desktop {

struct CameraAdvancedParametersManifestManager::Private
{
    QHash<QnUuid, QnCameraAdvancedParams> manifestsByCameraId;
};

CameraAdvancedParametersManifestManager::CameraAdvancedParametersManifestManager(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

CameraAdvancedParametersManifestManager::~CameraAdvancedParametersManifestManager()
{
}

std::optional<QnCameraAdvancedParams> CameraAdvancedParametersManifestManager::manifest(
    const QnVirtualCameraResourcePtr& camera) const
{
    if (!NX_ASSERT(camera))
        return std::nullopt;

    const auto iter = d->manifestsByCameraId.find(camera->getId());
    if (iter == d->manifestsByCameraId.cend())
        return std::nullopt;

    return *iter;
}

void CameraAdvancedParametersManifestManager::loadManifestAsync(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera))
        return;

    if (!NX_ASSERT(connection()))
        return;

    auto callback = nx::utils::guarded(this,
        [this, camera](bool success, int /*handle*/, nx::network::rest::JsonResult data)
        {
            if (!success)
                return;

            const auto manifest = data.deserialized<QnCameraAdvancedParams>();
            d->manifestsByCameraId[camera->getId()] = manifest;
            emit manifestLoaded(camera, manifest);
        });

    connectedServerApi()->getJsonResult(
        NX_FMT("/rest/v2/devices/%1/advanced/*/manifest", camera->getId()),
        /*params*/ {},
        callback,
        thread(),
        camera->getParentId());
}

} // namespace nx::vms::client::desktop
