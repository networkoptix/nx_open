#include "system_health_business_action.h"

#include <business/events/abstract_business_event.h>

#include <utils/common/synctime.h>


QnSystemHealthBusinessAction::QnSystemHealthBusinessAction(QnSystemHealth::MessageType message):
    base_type(QnBusinessParams())
{
    QnBusinessParams runtimeParams;
    QnBusinessEventRuntime::setEventType(&runtimeParams, BusinessEventType::Value(BusinessEventType::BE_SystemHealthMessage + message));
    QnBusinessEventRuntime::setEventTimestamp(&runtimeParams, qnSyncTime->currentUSecsSinceEpoch());
    setRuntimeParams(runtimeParams);

    QnBusinessParams actionParams;
    BusinessActionParameters::setUserGroup(&actionParams, 1);
    setParams(actionParams);

}
