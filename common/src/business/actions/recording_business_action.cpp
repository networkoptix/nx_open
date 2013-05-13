#include "recording_business_action.h"

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(BusinessActionType::CameraRecording, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return m_params.getFps();
}

QnStreamQuality QnRecordingBusinessAction::getStreamQuality() const {
    return m_params.getStreamQuality();
}

int QnRecordingBusinessAction::getRecordDuration() const {
    return m_params.getRecordDuration();
}

int QnRecordingBusinessAction::getRecordBefore() const {
    return m_params.getRecordBefore();
}

int QnRecordingBusinessAction::getRecordAfter() const {
    return m_params.getRecordAfter();
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
