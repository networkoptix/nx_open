
#include "instant_business_event.h"

QnInstantBusinessEvent::QnInstantBusinessEvent(QnBusiness::EventType eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, Qn::UndefinedState, timeStamp)
{
    Q_ASSERT(!QnBusiness::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::checkCondition(Qn::ToggleState state, const QnBusinessEventParameters &params) const {
    // Rule MUST contain 'Not Defined' event state filter
    Q_UNUSED(params)
    return (state == Qn::UndefinedState);
}
