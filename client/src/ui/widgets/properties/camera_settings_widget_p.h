#ifndef CAMERA_SETTINGS_WIDGET_P_H
#define CAMERA_SETTINGS_WIDGET_P_H

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>

/** Temporary class that will exist until full camera settings widget refactor */
class QnCameraSettingsWidgetPrivate
{
public:
    explicit QnCameraSettingsWidgetPrivate();
    virtual ~QnCameraSettingsWidgetPrivate();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    void calculateMaxFps(int *maxFps, int *maxDualStreamFps, Qn::MotionType motionTypeOverride = Qn::MT_Default);
private:
    QnVirtualCameraResourceList m_cameras;
};

#endif // CAMERA_SETTINGS_WIDGET_P_H
