#include "event_rules_notification_manager.h"

#include <nx/vms/api/data/event_rule_data.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/rule.h>

namespace ec2
{

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::EventActionData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::broadcastAction || tran.command == ApiCommand::execAction);
    nx::vms::event::AbstractActionPtr businessAction;
    fromApiToResource(tran.params, businessAction);
    businessAction->setReceivedFromRemoteHost(true);
    if (tran.command == ApiCommand::broadcastAction)
        emit gotBroadcastAction(businessAction);
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeEventRule);
    emit removed(QnUuid(tran.params.id));
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::EventRuleData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveEventRule);
    emit addedOrUpdated(tran.params, source);
}

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::ResetEventRulesData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::resetEventRules);
    emit businessRuleReset(tran.params.defaultRules);
}

} // namespace ec2
