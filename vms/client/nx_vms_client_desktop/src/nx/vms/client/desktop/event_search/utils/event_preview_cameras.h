// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <core/resource/resource_fwd.h>

class QModelIndex;

namespace nx::vms::client::desktop {

/** Whether the user can see a snapshot of the camera. previewTime <= 0 means live. */
NX_VMS_CLIENT_DESKTOP_API bool isPreviewAccessible(
    const QnVirtualCameraResourcePtr& camera, std::chrono::microseconds previewTime);

/** Camera to show on the tile: the first visible one, or null if none is visible. */
NX_VMS_CLIENT_DESKTOP_API QnVirtualCameraResourcePtr selectPreviewCamera(
    const QnVirtualCameraResourceList& cameras, std::chrono::microseconds previewTime);

/**
 * Cameras of the event whose snapshots the user can see. The tile camera goes first if the user
 * can see its snapshot.
 */
NX_VMS_CLIENT_DESKTOP_API QnVirtualCameraResourceList previewCameras(
    const QnVirtualCameraResourceList& cameras,
    const QnVirtualCameraResourcePtr& tileCamera,
    std::chrono::microseconds previewTime);

/** Number of snapshots for the tile counter. Zero if the tile doesn't show the first one. */
NX_VMS_CLIENT_DESKTOP_API int previewCount(const QnVirtualCameraResourceList& previewCameras,
    const QnVirtualCameraResourcePtr& tileCamera);

/** What the tile shows for its camera. */
enum class TileSnapshot
{
    /** No snapshot: a loader, a video stream or no camera at all. */
    none,
    /** The same full frame the tooltip would show. */
    fullFrame,
    /** A crop or an object best shot: the full frame is not seen on the tile. */
    partial,
};

/** What the tile of the model item shows for its camera, see EventRibbon. */
NX_VMS_CLIENT_DESKTOP_API TileSnapshot tileSnapshot(const QModelIndex& index);

/**
 * Snapshots for the tooltip of a tile. The tile camera is skipped only if the tile already shows
 * its full frame. If the tile camera is the only one, the list is empty: the tooltip shows the
 * tile snapshot enlarged instead.
 */
NX_VMS_CLIENT_DESKTOP_API QnVirtualCameraResourceList tooltipPreviewCameras(
    const QnVirtualCameraResourceList& previewCameras,
    const QnVirtualCameraResourcePtr& tileCamera,
    TileSnapshot tileSnapshot);

} // namespace nx::vms::client::desktop
