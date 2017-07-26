#include "fps_calculator.h"

// TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

namespace Qn {

void calculateMaxFps(
    const QnVirtualCameraResourceList& cameras,
    int *maxFps,
    int *maxDualStreamFps,
    MotionType motionTypeOverride)
{
    for (const auto &camera : cameras)
    {
        int cameraFps = camera->getMaxFps();
        int cameraDualStreamingFps = cameraFps;
        bool shareFps = camera->streamFpsSharingMethod() == Qn::BasicFpsSharing;
        Qn::MotionType motionType = motionTypeOverride == Qn::MT_Default
            ? camera->getMotionType()
            : motionTypeOverride;

        switch (motionType)
        {
            case Qn::MT_HardwareGrid:
                if (shareFps)
                    cameraDualStreamingFps -= MIN_SECOND_STREAM_FPS;
                break;
            case Qn::MT_SoftwareGrid:
                if (shareFps)
                {
                    cameraFps -= MIN_SECOND_STREAM_FPS;
                    cameraDualStreamingFps -= MIN_SECOND_STREAM_FPS;
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

QPair<int, int> calculateMaxFps(const QnVirtualCameraResourceList& cameras, MotionType motionTypeOverride)
{
    int maxFps = std::numeric_limits<int>::max();
    int maxDualStreamingFps = maxFps;
    calculateMaxFps(cameras, &maxFps, &maxDualStreamingFps, motionTypeOverride);
    return {maxFps, maxDualStreamingFps};
}

QPair<int, int> calculateMaxFps(const QnVirtualCameraResourcePtr& camera, MotionType motionTypeOverride)
{
    return calculateMaxFps(QnVirtualCameraResourceList() << camera, motionTypeOverride);
}

} //namespace Qn

