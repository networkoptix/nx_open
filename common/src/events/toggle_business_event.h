#ifndef __TOGGLE_BUSINESS_EVENT_H__
#define __TOGGLE_BUSINESS_EVENT_H__

#include "abstract_business_event.h"

class QnToggleBusinessEvent: public QnAbstractBusinessEvent
{
public:
    enum ToggleState {Off = 0, On = 1};

    QnToggleBusinessEvent(ToggleState state = Off);
    void setToggleState(ToggleState value) { m_toggleState = value; }
private:
    ToggleState m_toggleState;
};

typedef QSharedPointer<QnToggleBusinessEvent> QnToggleBusinessEventPtr;

#endif // __TOGGLE_BUSINESS_EVENT_H__
