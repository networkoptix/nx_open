#include "recording_business_action.h"

#include <business/business_action_parameters.h>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_resource.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(BusinessActionType::CameraRecording, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return m_params.getFps();
}

Qn::StreamQuality QnRecordingBusinessAction::getStreamQuality() const {
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

bool QnCameraRecordingAllowedPolicy::isResourceValid(const QnVirtualCameraResourcePtr &camera) {
    return !camera->isScheduleDisabled();
}

QString QnCameraRecordingAllowedPolicy::getErrorText(int invalid, int total) {
    return tr("Recording is disabled for %1 of %2 selected cameras.").arg(invalid).arg(total);
}
