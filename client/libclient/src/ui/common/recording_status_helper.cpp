#include "recording_status_helper.h"

#include <core/resource/camera_resource.h>

#include <ui/style/skin.h>
#include <utils/common/synctime.h>

// TODO: #Elric this should be a resource parameter that is updated from the server.
int QnRecordingStatusHelper::currentRecordingMode(const QnVirtualCameraResourcePtr &camera) {
    if(!camera || camera->isScheduleDisabled())
        return Qn::RT_Never;

    QDateTime dateTime = qnSyncTime->currentDateTime();
    int dayOfWeek = dateTime.date().dayOfWeek();
    int seconds = QTime(0, 0, 0, 0).secsTo(dateTime.time());

    foreach(const QnScheduleTask &task, camera->getScheduleTasks())
        if(task.getDayOfWeek() == dayOfWeek && task.getStartTime() <= seconds && seconds <= task.getEndTime())
            return task.getRecordingType();

    return Qn::RT_Never;
}

QString QnRecordingStatusHelper::tooltip(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RT_Never:
        return tr("Not recording");
    case Qn::RT_Always:
        return tr("Recording everything");
    case Qn::RT_MotionOnly:
        return tr("Recording motion only");
    case Qn::RT_MotionAndLowQuality:
        return tr("Recording motion and low quality");
    default:
        return QString();
    }
    return QString();
}

QString QnRecordingStatusHelper::shortTooltip(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RT_Never:
        return tr("Not recording");
    case Qn::RT_Always:
        return tr("Continuous");
    case Qn::RT_MotionOnly:
        return tr("Motion only");
    case Qn::RT_MotionAndLowQuality:
        return tr("Motion + Lo-Res");
    default:
        return QString();
    }
    return QString();
}

QIcon QnRecordingStatusHelper::icon(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RT_Never:
        return qnSkin->icon("item/recording_off.png");
    case Qn::RT_Always:
        return qnSkin->icon("item/recording.png");
    case Qn::RT_MotionOnly:
        return qnSkin->icon("item/recording_motion.png");
    case Qn::RT_MotionAndLowQuality:
        return qnSkin->icon("item/recording_motion_lq.png");
    default:
        return QIcon();
    }
    return QIcon();
}
