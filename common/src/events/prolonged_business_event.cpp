#include "prolonged_business_event.h"

QnProlongedBusinessEvent::QnProlongedBusinessEvent(BusinessEventType::Value eventType,
                                                   const QnResourcePtr& resource,
                                                   ToggleState::Value toggleState,
                                                   qint64 timeStamp):
    base_type(eventType, resource, toggleState, timeStamp)
{
    Q_ASSERT(BusinessEventType::hasToggleState(eventType));
}

bool QnProlongedBusinessEvent::checkCondition(ToggleState::Value state, const QnBusinessParams &params) const {
    Q_UNUSED(params)
    return state == ToggleState::NotDefined || state == getToggleState();
}
