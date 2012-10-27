#ifndef __TOGGLE_BUSINESS_ACTION_H__
#define __TOGGLE_BUSINESS_ACTION_H__

#include "abstract_business_action.h"
#include "business_logic_common.h"

class QnToggleBusinessAction: public QnAbstractBusinessAction
{
public:
    QnToggleBusinessAction(ToggleState state = ToggleState_Off);
    void setToggleState(ToggleState value) { m_toggleState = value; }
    ToggleState getToggleState() const { return m_toggleState; }
private:
    ToggleState m_toggleState;
};

typedef QSharedPointer<QnToggleBusinessAction> QnToggleBusinessActionPtr;

#endif // __TOGGLE_BUSINESS_EVENT_H__
