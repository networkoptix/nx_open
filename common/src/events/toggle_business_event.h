#ifndef __TOGGLE_BUSINESS_EVENT_H__
#define __TOGGLE_BUSINESS_EVENT_H__

#include "abstract_business_event.h"
#include "business_logic_common.h"

class QnToggleBusinessEvent: public QnAbstractBusinessEvent
{
public:
    QnToggleBusinessEvent(ToggleState state = ToggleState_Off);
    void setToggleState(ToggleState value) { m_toggleState = value; }
    ToggleState getToggleState() const { return m_toggleState; }
private:
    ToggleState m_toggleState;
};

typedef QSharedPointer<QnToggleBusinessEvent> QnToggleBusinessEventPtr;

#endif // __TOGGLE_BUSINESS_EVENT_H__
