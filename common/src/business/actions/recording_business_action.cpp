#include "recording_business_action.h"

#include <business/business_action_parameters.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::CameraRecordingAction, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return m_params.fps;
}

Qn::StreamQuality QnRecordingBusinessAction::getStreamQuality() const {
    return m_params.streamQuality;
}

int QnRecordingBusinessAction::getRecordDuration() const {
    return m_params.recordingDuration;
}

int QnRecordingBusinessAction::getRecordAfter() const {
    return m_params.recordAfter;
}
