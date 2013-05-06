#include "recording_business_action.h"

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::CameraRecording, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return QnBusinessActionParameters::getFps(getParams());
}

QnStreamQuality QnRecordingBusinessAction::getStreamQuality() const {
    return QnBusinessActionParameters::getStreamQuality(getParams());
}

int QnRecordingBusinessAction::getRecordDuration() const {
    return QnBusinessActionParameters::getRecordDuration(getParams());
}

int QnRecordingBusinessAction::getRecordBefore() const {
    return QnBusinessActionParameters::getRecordBefore(getParams());
}

int QnRecordingBusinessAction::getRecordAfter() const {
    return QnBusinessActionParameters::getRecordAfter(getParams());
}

bool QnRecordingBusinessAction::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

bool QnRecordingBusinessAction::isResourcesListValid(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    if (cameras.isEmpty())
        return false;
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            return false;
        }
    }
    return true;
}

int QnRecordingBusinessAction::invalidResourcesCount(const QnResourceList &resources) {
    QnVirtualCameraResourceList cameras = resources.filtered<QnVirtualCameraResource>();
    int invalid = 0;
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (!isResourceValid(camera)) {
            invalid++;
        }
    }
    return invalid;
}
