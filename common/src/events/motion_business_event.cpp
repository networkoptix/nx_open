#include "motion_business_event.h"

QnMotionBusinessEvent::QnMotionBusinessEvent():
    QnToggleBusinessEvent()
{
    setEventType(BE_Camera_Motion);
}

/*
QByteArray QnMotionBusinessEvent::serialize()
{
    // todo: implement me
    return QByteArray();
}

bool QnMotionBusinessEvent::deserialize(const QByteArray& data)
{
    // todo: implement me
    return true;
}
*/

bool QnMotionBusinessEvent::checkCondition(const QnBusinessParams& params) const
{
    return true;
}
