#include "prolonged_business_event.h"
#include <business/actions/abstract_business_action.h>

QnProlongedBusinessEvent::QnProlongedBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp):
    base_type(eventType, resource, toggleState, timeStamp)
{
    Q_ASSERT(QnBusiness::hasToggleState(eventType));
}

bool QnProlongedBusinessEvent::checkCondition(QnBusiness::EventState state, const QnBusinessEventParameters &params, QnBusiness::ActionType actionType) const {
    Q_UNUSED(params)
    return state == QnBusiness::UndefinedState || state == getToggleState() || QnBusiness::hasToggleState(actionType);
}
