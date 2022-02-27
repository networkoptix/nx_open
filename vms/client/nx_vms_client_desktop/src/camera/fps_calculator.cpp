// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "fps_calculator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int* maxFps,
    int* maxDualStreamFps,
    bool motionDetectionAllowed)
{
    using namespace nx::vms::api;

    if (maxFps)
        *maxFps = std::numeric_limits<int>::max();
    if (maxDualStreamFps)
        *maxDualStreamFps = std::numeric_limits<int>::max();

    for (const auto& camera: cameras)
    {
        int cameraFps = camera->getStatus() != nx::vms::api::ResourceStatus::unauthorized
            ? camera->getMaxFps()
            : QnSecurityCamResource::kDefaultMaxFps;

        int cameraDualStreamingFps = cameraFps;
        const int reservedSecondStreamFps = camera->reservedSecondStreamFps();
        const auto motionType = motionDetectionAllowed
            ? camera->getMotionType()
            : MotionType::none;

        switch (motionType)
        {
            case MotionType::hardware:
                cameraDualStreamingFps -= reservedSecondStreamFps;
                break;
            case MotionType::software:
                cameraFps -= reservedSecondStreamFps;
                cameraDualStreamingFps -= reservedSecondStreamFps;
                break;
            default:
                break;
        }

        if (maxFps)
            *maxFps = qMin(*maxFps, cameraFps);
        if (maxDualStreamFps)
            *maxDualStreamFps = qMin(*maxDualStreamFps, cameraDualStreamingFps);
    }
}

QPair<int, int> calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    bool motionDetectionAllowed)
{
    int maxFps = std::numeric_limits<int>::max();
    int maxDualStreamingFps = maxFps;
    calculateMaxFps(cameras, &maxFps, &maxDualStreamingFps, motionDetectionAllowed);
    return {maxFps, maxDualStreamingFps};
}

QPair<int, int> calculateMaxFps(
    const QnVirtualCameraResourcePtr& camera,
    bool motionDetectionAllowed)
{
    return calculateMaxFps(QnVirtualCameraResourceList() << camera, motionDetectionAllowed);
}

} //namespace Qn

