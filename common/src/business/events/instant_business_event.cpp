
#include "instant_business_event.h"

#include <nx/utils/log/assert.h>

QnInstantBusinessEvent::QnInstantBusinessEvent(QnBusiness::EventType eventType,
                                               const QnResourcePtr& resource,
                                               qint64 timeStamp):
    base_type(eventType, resource, QnBusiness::UndefinedState, timeStamp)
{
    NX_ASSERT(!QnBusiness::hasToggleState(eventType));
}

bool QnInstantBusinessEvent::isEventStateMatched(QnBusiness::EventState state, QnBusiness::ActionType actionType) const {
    // Rule MUST contain 'Not Defined' event state filter
    Q_UNUSED(actionType)
    return (state == QnBusiness::UndefinedState);
}
