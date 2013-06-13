#include "recording_status_helper.h"
#include "ui/workbench/workbench_context.h"
#include "core/resource/media_resource.h"
#include "core/misc/schedule_recording_type.h"
#include "utils/common/synctime.h"
#include "ui/workbench/watchers/workbench_server_time_watcher.h"
#include "ui/style/skin.h"

int QnRecordingStatusHelper::currentRecordingMode(QnWorkbenchContext *context, QnVirtualCameraResourcePtr camera) 
{
    if(!camera)
        return Qn::RecordingType_Never;

    // TODO: #Elric this should be a resource parameter that is update from the server.
    QnMediaResourcePtr mediaRes = camera.dynamicCast<QnMediaResource>();
    QDateTime dateTime = qnSyncTime->currentDateTime().addMSecs(context->instance<QnWorkbenchServerTimeWatcher>()->localOffset(mediaRes, 0));
    int dayOfWeek = dateTime.date().dayOfWeek();
    int seconds = QTime().secsTo(dateTime.time());

    foreach(const QnScheduleTask &task, camera->getScheduleTasks())
        if(task.getDayOfWeek() == dayOfWeek && task.getStartTime() <= seconds && seconds <= task.getEndTime())
            return task.getRecordingType();

    return Qn::RecordingType_Never;
}

QString QnRecordingStatusHelper::tooltip(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RecordingType_Never:
        return QObject::tr("Not recording");
    case Qn::RecordingType_Run:
        return QObject::tr("Recording everything");
    case Qn::RecordingType_MotionOnly:
        return QObject::tr("Recording motion only");
    case Qn::RecordingType_MotionPlusLQ:
        return QObject::tr("Recording motion and low quality");
    default:
        return QString();
    }
    return QString();
}

QString QnRecordingStatusHelper::shortTooltip(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RecordingType_Never:
        return QObject::tr("Not recording");
    case Qn::RecordingType_Run:
        return QObject::tr("Everything");
    case Qn::RecordingType_MotionOnly:
        return QObject::tr("Motion only");
    case Qn::RecordingType_MotionPlusLQ:
        return QObject::tr("Motion + Lo-Res");
    default:
        return QString();
    }
    return QString();
}

QIcon QnRecordingStatusHelper::icon(int recordingMode)
{
    switch(recordingMode) {
    case Qn::RecordingType_Never:
        return qnSkin->icon("item/recording_off.png");
    case Qn::RecordingType_Run:
        return qnSkin->icon("item/recording.png");
    case Qn::RecordingType_MotionOnly:
        return qnSkin->icon("item/recording_motion.png");
    case Qn::RecordingType_MotionPlusLQ:
        return qnSkin->icon("item/recording_motion_lq.png");
    default:
        return QIcon();
    }
    return QIcon();
}
