
#include "instant_business_event.h"

QnInstantBusinessEvent::QnInstantBusinessEvent(QnBusiness::EventType eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, QnBusiness::UndefinedState, timeStamp)
{
    Q_ASSERT(!QnBusiness::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params, QnBusiness::ActionType actionType) const {
    // Rule MUST contain 'Not Defined' event state filter
    Q_UNUSED(params)
    Q_UNUSED(actionType)
    return (state == QnBusiness::UndefinedState);
}
