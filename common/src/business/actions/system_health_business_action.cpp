#include "system_health_business_action.h"

#include <business/events/abstract_business_event.h>
#include <business/business_action_parameters.h>

#include <utils/common/synctime.h>


QnSystemHealthBusinessAction::QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, const QnUuid& eventResourceId):
    base_type(QnBusiness::ShowPopupAction, QnBusinessEventParameters())
{
    QnBusinessEventParameters runtimeParams;
    runtimeParams.eventType = QnBusiness::EventType(QnBusiness::SystemHealthEvent + message);
    runtimeParams.eventTimestamp = qnSyncTime->currentUSecsSinceEpoch();
    runtimeParams.eventResourceId = eventResourceId;
    setRuntimeParams(runtimeParams);

    QnBusinessActionParameters actionParams;
    actionParams.userGroup = QnBusiness::AdminOnly;
    setParams(actionParams);

}
