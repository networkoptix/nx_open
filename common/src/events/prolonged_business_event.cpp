#include "prolonged_business_event.h"

QnProlongedBusinessEvent::QnProlongedBusinessEvent(BusinessEventType::Value eventType,
                                                   const QnResourcePtr& resource,
                                                   ToggleState::Value toggleState,
                                                   qint64 timeStamp):
    base_type(eventType, resource, toggleState, timeStamp)
{
    Q_ASSERT(BusinessEventType::hasToggleState(eventType));
}

bool QnProlongedBusinessEvent::checkCondition(const QnBusinessParams &params) const {
    ToggleState::Value toggleState = BusinessEventParameters::getToggleState(params);
    return toggleState == ToggleState::NotDefined || toggleState == getToggleState();
}
