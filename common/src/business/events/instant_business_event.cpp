#include "instant_business_event.h"

QnInstantBusinessEvent::QnInstantBusinessEvent(BusinessEventType::Value eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, ToggleState::NotDefined, timeStamp)
{
    Q_ASSERT(!BusinessEventType::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::checkCondition(ToggleState::Value state, const QnBusinessParams &params) const {
    // Rule MUST contain 'Not Defined' event state filter
    Q_UNUSED(params)
    return (state == ToggleState::NotDefined);
}
