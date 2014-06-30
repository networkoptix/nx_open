#include "camera_settings_widget_p.h"

//TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
#include <core/dataprovider/live_stream_provider.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnCameraSettingsWidgetPrivate::QnCameraSettingsWidgetPrivate() {
}

QnCameraSettingsWidgetPrivate::~QnCameraSettingsWidgetPrivate() {
}

const QnVirtualCameraResourceList &QnCameraSettingsWidgetPrivate::cameras() const {
    return m_cameras;
}

void QnCameraSettingsWidgetPrivate::setCameras(const QnVirtualCameraResourceList &cameras) {
    m_cameras = cameras;
}

void QnCameraSettingsWidgetPrivate::calculateMaxFps(int *maxFps, int *maxDualStreamFps, Qn::MotionType motionTypeOverride) {
    foreach (QnVirtualCameraResourcePtr camera, m_cameras)
    {
        int cameraFps = camera->getMaxFps();
        int cameraDualStreamingFps = cameraFps;
        bool shareFps = camera->streamFpsSharingMethod() == Qn::BasicFpsSharing;
        Qn::MotionType motionType = motionTypeOverride == Qn::MT_Default
                ? camera->getMotionType()
                : motionTypeOverride;

        switch (motionType) {
        case Qn::MT_HardwareGrid:
            if (shareFps)
                cameraDualStreamingFps -= MIN_SECOND_STREAM_FPS;
            break;
        case Qn::MT_SoftwareGrid:
            if (shareFps) {
                cameraFps -= MIN_SECOND_STREAM_FPS;
                cameraDualStreamingFps -= MIN_SECOND_STREAM_FPS;
            }
            break;
        default:
            break;
        }

        *maxFps = qMin(*maxFps, cameraFps);
        *maxDualStreamFps = qMin(*maxFps, cameraDualStreamingFps);
    }
}
