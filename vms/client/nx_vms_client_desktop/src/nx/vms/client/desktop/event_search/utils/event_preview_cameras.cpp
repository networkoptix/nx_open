// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_preview_cameras.h"

#include <QtCore/QModelIndex>
#include <QtCore/QRectF>

#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

bool isPreviewAccessible(const QnVirtualCameraResourcePtr& camera, microseconds previewTime)
{
    if (!NX_ASSERT(camera) || camera->hasFlags(Qn::ResourceFlag::fake))
        return false;

    // Assuming the rights were checked before sending the notification.
    if (camera->hasFlags(Qn::ResourceFlag::cross_system))
        return true;

    const auto requiredPermission =
        previewTime > 0us ? Qn::ViewFootagePermission : Qn::ViewLivePermission;

    return ResourceAccessManager::hasPermissions(camera, requiredPermission);
}

QnVirtualCameraResourcePtr selectPreviewCamera(
    const QnVirtualCameraResourceList& cameras, microseconds previewTime)
{
    for (const auto& camera: cameras)
    {
        if (camera && isPreviewAccessible(camera, previewTime))
            return camera;
    }

    return QnVirtualCameraResourcePtr();
}

QnVirtualCameraResourceList previewCameras(const QnVirtualCameraResourceList& cameras,
    const QnVirtualCameraResourcePtr& tileCamera,
    microseconds previewTime)
{
    QnVirtualCameraResourceList result;
    if (tileCamera && isPreviewAccessible(tileCamera, previewTime))
        result.push_back(tileCamera);

    for (const auto& camera: cameras)
    {
        if (camera && camera != tileCamera && isPreviewAccessible(camera, previewTime))
            result.push_back(camera);
    }

    return result;
}

int previewCount(const QnVirtualCameraResourceList& previewCameras,
    const QnVirtualCameraResourcePtr& tileCamera)
{
    return tileCamera && previewCameras.value(0) == tileCamera ? previewCameras.size() : 0;
}

TileSnapshot tileSnapshot(const QModelIndex& index)
{
    if (index.data(Qn::ForcePreviewLoaderRole).toBool()
        || index.data(Qn::ShowVideoPreviewRole).toBool())
    {
        return TileSnapshot::none;
    }

    const bool isPartial = !index.data(core::ItemZoomRectRole).value<QRectF>().isEmpty()
        || !index.data(core::ObjectTrackIdRole).value<nx::Uuid>().isNull();

    return isPartial ? TileSnapshot::partial : TileSnapshot::fullFrame;
}

QnVirtualCameraResourceList tooltipPreviewCameras(
    const QnVirtualCameraResourceList& previewCameras,
    const QnVirtualCameraResourcePtr& tileCamera,
    TileSnapshot tileSnapshot)
{
    const bool tileCameraIsFirst =
        tileSnapshot != TileSnapshot::none && tileCamera && previewCameras.value(0) == tileCamera;

    if (!tileCameraIsFirst)
        return previewCameras;

    if (previewCameras.size() < 2)
        return {};

    if (tileSnapshot == TileSnapshot::partial)
        return previewCameras;

    return QnVirtualCameraResourceList(previewCameras.begin() + 1, previewCameras.end());
}

} // namespace nx::vms::client::desktop
