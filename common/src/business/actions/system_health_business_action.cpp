#include "system_health_business_action.h"

#include <business/events/abstract_business_event.h>
#include <business/business_action_parameters.h>

#include <utils/common/synctime.h>


QnSystemHealthBusinessAction::QnSystemHealthBusinessAction(QnSystemHealth::MessageType message, const QUuid& eventResourceId):
    base_type(QnBusiness::ShowPopupAction, QnBusinessEventParameters())
{
    QnBusinessEventParameters runtimeParams;
    runtimeParams.setEventType(QnBusiness::EventType(QnBusiness::SystemHealthEvent + message));
    runtimeParams.setEventTimestamp(qnSyncTime->currentUSecsSinceEpoch());
    runtimeParams.setEventResourceId(eventResourceId);
    setRuntimeParams(runtimeParams);

    QnBusinessActionParameters actionParams;
    actionParams.setUserGroup(QnBusinessActionParameters::AdminOnly);
    setParams(actionParams);

}
