#include "business_event_notification_manager.h"

#include <nx_ec/data/api_business_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/rule.h>

namespace ec2
{

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<ApiBusinessActionData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::broadcastAction || tran.command == ApiCommand::execAction);
    nx::vms::event::AbstractActionPtr businessAction;
    fromApiToResource(tran.params, businessAction);
    businessAction->setReceivedFromRemoteHost(true);
    if (tran.command == ApiCommand::broadcastAction)
        emit gotBroadcastAction(businessAction);
    else
        emit execBusinessAction(businessAction);
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeEventRule);
    emit removed(QnUuid(tran.params.id));
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<ApiBusinessRuleData>& tran, 
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveEventRule);
    nx::vms::event::RulePtr businessRule(new nx::vms::event::Rule());
    fromApiToResource(tran.params, businessRule);
    emit addedOrUpdated(businessRule, source);
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<ApiResetBusinessRuleData>& tran, 
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::resetEventRules);
    emit businessRuleReset(tran.params.defaultRules);
}

} // namespace ec2
