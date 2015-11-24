#include "prolonged_business_event.h"
#include <business/actions/abstract_business_action.h>

QnProlongedBusinessEvent::QnProlongedBusinessEvent(QnBusiness::EventType eventType, const QnResourcePtr& resource, QnBusiness::EventState toggleState, qint64 timeStamp):
    base_type(eventType, resource, toggleState, timeStamp)
{
    Q_ASSERT(QnBusiness::hasToggleState(eventType));
}

bool QnProlongedBusinessEvent::isEventStateMatched(QnBusiness::EventState state, QnBusiness::ActionType actionType) const {
    return state == QnBusiness::UndefinedState || state == getToggleState() || QnBusiness::hasToggleState(actionType);
}
