#include "instant_business_event.h"

QnInstantBusinessEvent::QnInstantBusinessEvent(BusinessEventType::Value eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, ToggleState::NotDefined, timeStamp)
{
    Q_ASSERT(!BusinessEventType::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::checkCondition(const QnBusinessParams &params) const {
    // Rule MUST contain 'Not Defined' event state filter
    ToggleState::Value toggleState = BusinessEventParameters::getToggleState(params);
    return (toggleState == ToggleState::NotDefined);
}
