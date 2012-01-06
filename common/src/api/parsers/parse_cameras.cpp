#include "parse_cameras.h"

QnApiCameraPtr unparseCamera(const QnCameraResource& cameraIn)
{
    QnApiCameraPtr camera(new xsd::api::cameras::Camera(cameraIn.getId().toString().toStdString(),
                                     cameraIn.getName().toStdString(),
                                     cameraIn.getTypeId().toString().toStdString(),
                                     cameraIn.getUrl().toStdString(),
                                     cameraIn.getMAC().toString().toStdString(),
                                     cameraIn.getAuth().user().toStdString(),
                                     cameraIn.getAuth().password().toStdString()));

    xsd::api::scheduleTasks::ScheduleTasks scheduleTasks;
    foreach(const QnScheduleTask& scheduleTaskIn, cameraIn.getScheduleTasks())
    {
        xsd::api::scheduleTasks::ScheduleTask scheduleTask(
                                                scheduleTaskIn.getStartTime(),
                                                scheduleTaskIn.getEndTime(),
                                                scheduleTaskIn.getDoRecordAudio(),
                                                scheduleTaskIn.getRecordingType(),
                                                scheduleTaskIn.getDayOfWeek(),
                                                scheduleTaskIn.getBeforeThreshold(),
                                                scheduleTaskIn.getAfterThreshold(),
                                                scheduleTaskIn.getStreamQuality(),
                                                scheduleTaskIn.getFps());

        scheduleTasks.scheduleTask().push_back(scheduleTask);
    }

    camera->scheduleTasks(scheduleTasks);
    camera->parentID(cameraIn.getParentId().toString().toStdString());

    return camera;
}
