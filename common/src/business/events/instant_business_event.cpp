
#include "instant_business_event.h"

QnInstantBusinessEvent::QnInstantBusinessEvent(QnBusiness::EventType eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, QnBusiness::UndefinedState, timeStamp)
{
    Q_ASSERT(!QnBusiness::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::isEventStateMatched(QnBusiness::EventState state, QnBusiness::ActionType actionType) const {
    // Rule MUST contain 'Not Defined' event state filter
    Q_UNUSED(actionType)
    return (state == QnBusiness::UndefinedState);
}
