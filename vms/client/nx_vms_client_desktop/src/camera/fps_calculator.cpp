// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fps_calculator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    float* maxFps)
{
    using namespace nx::vms::api;

    if (maxFps)
        *maxFps = std::numeric_limits<float>::max();

    for (const auto& camera: cameras)
    {
        auto cameraFps = camera->getStatus() != nx::vms::api::ResourceStatus::unauthorized
            ? camera->getMaxFps()
            : QnVirtualCameraResource::kDefaultMaxFps;

        if (camera->hasDualStreaming())
            cameraFps -= camera->reservedSecondStreamFps();
        if (maxFps)
            *maxFps = std::min(*maxFps, cameraFps);
    }
}

float calculateMaxFps(
    const QnVirtualCameraResourceList& cameras)
{
    float maxFps = std::numeric_limits<float>::max();
    calculateMaxFps(cameras, &maxFps);
    return maxFps;
}

float calculateMaxFps(
    const QnVirtualCameraResourcePtr& camera)
{
    return calculateMaxFps(QnVirtualCameraResourceList() << camera);
}

} //namespace Qn
