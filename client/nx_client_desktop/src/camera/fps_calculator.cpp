#include "fps_calculator.h"

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/dataprovider/live_stream_params.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int* maxFps,
    int* maxDualStreamFps,
    bool motionDetectionAllowed)
{
    if (maxFps)
        *maxFps = std::numeric_limits<int>::max();
    if (maxDualStreamFps)
        *maxDualStreamFps = std::numeric_limits<int>::max();

    for (const auto& camera: cameras)
    {
        int cameraFps = camera->getMaxFps();
        int cameraDualStreamingFps = cameraFps;
        const bool shareFps = camera->streamFpsSharingMethod() == Qn::BasicFpsSharing;
        const auto motionType = motionDetectionAllowed
            ? camera->getMotionType()
            : Qn::MotionType::MT_NoMotion;

        switch (motionType)
        {
            case Qn::MotionType::MT_HardwareGrid:
                if (shareFps)
                    cameraDualStreamingFps -= QnLiveStreamParams::kMinSecondStreamFps;
                break;
            case Qn::MotionType::MT_SoftwareGrid:
                if (shareFps)
                {
                    cameraFps -= QnLiveStreamParams::kMinSecondStreamFps;
                    cameraDualStreamingFps -= QnLiveStreamParams::kMinSecondStreamFps;
                }
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

