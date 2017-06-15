#include "recording_business_action.h"

#include <business/business_action_parameters.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::CameraRecordingAction, runtimeParams)
{
}

int QnRecordingBusinessAction::getFps() const {
    return m_params.fps;
}

Qn::StreamQuality QnRecordingBusinessAction::getStreamQuality() const
{
    return m_params.streamQuality;
}

int QnRecordingBusinessAction::getDurationSec() const
{
    return m_params.durationMs / 1000;
}

int QnRecordingBusinessAction::getRecordAfterSec() const
{
    return m_params.recordAfter;
}
