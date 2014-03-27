#include "system_health_business_action.h"

#include <business/events/abstract_business_event.h>
#include <business/business_action_parameters.h>

#include <utils/common/synctime.h>


QnSystemHealthBusinessAction::QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, const QnId& eventResourceId):
    base_type(BusinessActionType::ShowPopup, QnBusinessEventParameters())
{
    QnBusinessEventParameters runtimeParams;
    runtimeParams.setEventType(BusinessEventType::Value(BusinessEventType::SystemHealthMessage + message));
    runtimeParams.setEventTimestamp(qnSyncTime->currentUSecsSinceEpoch());
    runtimeParams.setEventResourceId(eventResourceId);
    setRuntimeParams(runtimeParams);

    QnBusinessActionParameters actionParams;
    actionParams.setUserGroup(QnBusinessActionParameters::AdminOnly);
    setParams(actionParams);

}
