// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fps_calculator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int* maxFps)
{
    using namespace nx::vms::api;

    if (maxFps)
        *maxFps = std::numeric_limits<int>::max();

    for (const auto& camera: cameras)
    {
        int cameraFps = camera->getStatus() != nx::vms::api::ResourceStatus::unauthorized
            ? camera->getMaxFps()
            : QnSecurityCamResource::kDefaultMaxFps;

        if (camera->hasDualStreaming())
            cameraFps -= camera->reservedSecondStreamFps();
        if (maxFps)
            *maxFps = qMin(*maxFps, cameraFps);
    }
}

int calculateMaxFps(
    const QnVirtualCameraResourceList& cameras)
{
    int maxFps = std::numeric_limits<int>::max();
    calculateMaxFps(cameras, &maxFps);
    return maxFps;
}

int calculateMaxFps(
    const QnVirtualCameraResourcePtr& camera)
{
    return calculateMaxFps(QnVirtualCameraResourceList() << camera);
}

} //namespace Qn
