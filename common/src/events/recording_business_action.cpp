#include "recording_business_action.h"

QnRecordingBusinessAction::QnRecordingBusinessAction():
    base_type(BusinessActionType::BA_CameraRecording)
{
    setFps(10);
    setStreamQuality(QnQualityHighest);
}
