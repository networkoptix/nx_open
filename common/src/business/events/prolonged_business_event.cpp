#include "prolonged_business_event.h"

QnProlongedBusinessEvent::QnProlongedBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, Qn::ToggleState toggleState, qint64 timeStamp):
    base_type(eventType, resource, toggleState, timeStamp)
{
    Q_ASSERT(QnBusiness::hasToggleState(eventType));
}

bool QnProlongedBusinessEvent::checkCondition(Qn::ToggleState state, const QnBusinessEventParameters &params) const {
    Q_UNUSED(params)
    return state == Qn::UndefinedState || state == getToggleState();
}
