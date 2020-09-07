#include "camera_advanced_parameters_manifest_manager.h"

#include <api/server_rest_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

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

    const QnMediaServerResourcePtr server = camera->getParentServer();
    if (!NX_ASSERT(server))
        return;

    const auto connection = server->restConnection();

    QnRequestParamList params;
    params.insert("cameraId", camera->getId().toString());

    auto callback = nx::utils::guarded(this,
        [this, camera](bool success, int /*handle*/, QnJsonRestResult data)
        {
            if (!success)
                return;

            const auto manifest = data.deserialized<QnCameraAdvancedParams>();
            d->manifestsByCameraId[camera->getId()] = manifest;
            emit manifestLoaded(camera, manifest);
        });

    connection->getJsonResult("/api/getCameraParamManifest", params, callback, thread());
}

} // namespace nx::vms::client::desktop
