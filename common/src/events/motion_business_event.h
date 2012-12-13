#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include "abstract_business_event.h"

class QnMotionBusinessEvent: public QnAbstractBusinessEvent
{
public:
    QnMotionBusinessEvent(QnResourcePtr resource,
                          ToggleState::Value toggleState,
                          qint64 timeStamp);

};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
