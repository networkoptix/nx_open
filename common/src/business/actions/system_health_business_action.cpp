#include "system_health_business_action.h"

#include <business/events/abstract_business_event.h>
#include <business/business_action_parameters.h>

#include <utils/common/synctime.h>


QnSystemHealthBusinessAction::QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, int eventResourceId):
    base_type(BusinessActionType::ShowPopup, QnBusinessParams())
{
    QnBusinessParams runtimeParams;
    QnBusinessEventRuntime::setEventType(&runtimeParams, BusinessEventType::Value(BusinessEventType::SystemHealthMessage + message));
    QnBusinessEventRuntime::setEventTimestamp(&runtimeParams, qnSyncTime->currentUSecsSinceEpoch());
    QnBusinessEventRuntime::setEventResourceId(&runtimeParams, eventResourceId);
    setRuntimeParams(runtimeParams);

    QnBusinessParams actionParams;
    QnBusinessActionParameters::setUserGroup(&actionParams, QnBusinessActionParameters::AdminOnly);
    setParams(actionParams);

}
