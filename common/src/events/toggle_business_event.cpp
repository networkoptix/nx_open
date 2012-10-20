#include "toggle_business_event.h"

QnToggleBusinessEvent::QnToggleBusinessEvent(ToggleState state):
    QnAbstractBusinessEvent(),
    m_toggleState(state)
{

}
