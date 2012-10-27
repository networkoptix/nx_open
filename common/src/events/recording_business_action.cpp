#include "recording_business_action.h"
#include "toggle_business_event.h"

QnRecordingBusinessAction::QnRecordingBusinessAction()
{
    setActionType(BA_CameraRecording);
    setFps(10);
    setStreamQuality(QnQualityHighest);
}

QByteArray QnRecordingBusinessAction::serialize()
{
    // todo: implement me!
    return QByteArray();
}

bool QnRecordingBusinessAction::deserialize(const QByteArray& data)
{
    // todo: implement me!
    return true;
}
