#ifndef __MOTION_BUSINESS_EVENT_H__
#define __MOTION_BUSINESS_EVENT_H__

#include "toggle_business_event.h"

class QnMotionBusinessEvent: public QnToggleBusinessEvent
{
public:
    QnMotionBusinessEvent();
protected:
    virtual QByteArray serialize() override;
    virtual bool deserialize(const QByteArray& data) override;
};

typedef QSharedPointer<QnMotionBusinessEvent> QnMotionBusinessEventPtr;

#endif // __MOTION_BUSINESS_EVENT_H__
