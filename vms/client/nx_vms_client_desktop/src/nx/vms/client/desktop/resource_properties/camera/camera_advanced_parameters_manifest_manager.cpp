// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_advanced_parameters_manifest_manager.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/rest/params.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

struct CameraAdvancedParametersManifestManager::Private
{
    // Store manifests based on camera id, knowing id will not differ between various systems.
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

rest::Handle CameraAdvancedParametersManifestManager::loadManifestAsync(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera))
        return 0;

    auto systemContext = SystemContext::fromResource(camera);
    if (!NX_ASSERT(systemContext))
        return 0;

    auto api = systemContext->connectedServerApi();
    if (!NX_ASSERT(api))
        return 0;

    auto callback = nx::utils::guarded(this,
        [this, camera](bool success, auto handle, auto result, auto /*headers*/)
        {
            if (!success)
            {
                NX_WARNING(this, "Manifest was not loaded for %1", camera);
                emit manifestLoadingFailed(handle, camera);
                return;
            }

            NX_VERBOSE(this, "Manifest loaded successfully for %1", camera);
            QnCameraAdvancedParams manifest;
            QJson::deserialize(result, &manifest);
            d->manifestsByCameraId[camera->getId()] = manifest;
            NX_VERBOSE(this, "Groups count is %1", manifest.groups.size());

            emit manifestLoaded(handle, camera, manifest);
        });

    NX_VERBOSE(this, "Requesting analytics manifest for camera %1", camera);
    return api->getRawResult(
        NX_FMT("/rest/v2/devices/%1/advanced/*/manifest", camera->getId()),
        /*params*/ {},
        callback,
        thread());
}

} // namespace nx::vms::client::desktop
