// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_support.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

namespace nx::vms::common {
namespace camera_hotspots {

bool supportsCameraHotspots(const QnResourcePtr& cameraResource)
{
    const auto camera = cameraResource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return false;

    return !camera->flags().testFlag(Qn::desktop_camera)
        && camera->hasVideo();
}

bool hotspotCanReferToResource(const QnResourcePtr& targetResource)
{
    if (const auto layout = targetResource.dynamicCast<QnLayoutResource>())
    {
        return !layout->isServiceLayout() && layout->isShared();
    }
    else if (const auto camera = targetResource.dynamicCast<QnVirtualCameraResource>())
    {
        const auto cameraFlags = camera->flags();

        return !cameraFlags.testFlag(Qn::desktop_camera)
            && !cameraFlags.testFlag(Qn::virtual_camera)
            && cameraFlags.testFlag(Qn::server_live_cam)
            && camera->hasVideo();
    }
    return false;
}

} // namespace camera_hotspots
} // nx::vms::common
