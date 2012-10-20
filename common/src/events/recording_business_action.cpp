#include "recording_business_action.h"
#include "toggle_business_event.h"

QnRecordingBusinessAction::QnRecordingBusinessAction()
{
    setActionType(BA_CameraRecording);
}

bool QnRecordingBusinessAction::execute()
{
    // todo: implement me!
    return true;
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
