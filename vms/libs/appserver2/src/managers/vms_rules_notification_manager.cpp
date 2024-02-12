// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_notification_manager.h"

#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/event/actions/abstract_action.h>

namespace ec2
{

void VmsRulesNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::rules::Rule>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveVmsRule);
    emit ruleUpdated(tran.params, source);
}

void VmsRulesNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeVmsRule);
    emit ruleRemoved(nx::Uuid(tran.params.id));
}

void VmsRulesNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::rules::ResetRules>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::resetVmsRules);
    emit reset();
}

void VmsRulesNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::rules::EventInfo>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::broadcastVmsEvent
        || tran.command == ApiCommand::transmitVmsEvent);

    // TODO: #spanasenko Check action type.
    emit eventReceived(tran.params);
}

} // namespace ec2
