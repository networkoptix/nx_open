#ifndef __TOGGLE_BUSINESS_ACTION_H__
#define __TOGGLE_BUSINESS_ACTION_H__

#include "abstract_business_action.h"

class QnToggleBusinessAction: public QnAbstractBusinessAction
{
public:
    enum ToggleState {Off = 0, On = 1};

    QnToggleBusinessAction(ToggleState state = Off);
    void setToggleState(ToggleState value) { m_toggleState = value; }
    ToggleState toggleState() const { return m_toggleState; }
private:
    ToggleState m_toggleState;
};

typedef QSharedPointer<QnToggleBusinessAction> QnToggleBusinessActionPtr;

#endif // __TOGGLE_BUSINESS_EVENT_H__
