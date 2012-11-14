#include "recording_business_action.h"

QnRecordingBusinessAction::QnRecordingBusinessAction()
{
    setActionType(BA_CameraRecording);
    setFps(10);
    setStreamQuality(QnQualityHighest);
}
