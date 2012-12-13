#include "recording_business_action.h"

QnRecordingBusinessAction::QnRecordingBusinessAction()
{
    setActionType(BusinessActionType::BA_CameraRecording);
    setFps(10);
    setStreamQuality(QnQualityHighest);
}
