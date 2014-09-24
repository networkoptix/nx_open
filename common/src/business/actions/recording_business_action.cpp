#include "recording_business_action.h"

#include <business/business_action_parameters.h>

QnRecordingBusinessAction::QnRecordingBusinessAction(const QnBusinessEventParameters &runtimeParams):
    base_type(QnBusiness::CameraRecordingAction, runtimeParams)
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
