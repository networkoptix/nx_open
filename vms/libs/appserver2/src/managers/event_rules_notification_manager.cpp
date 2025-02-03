// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_rules_notification_manager.h"

namespace ec2
{

void QnBusinessEventNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeEventRule);
    emit removed(nx::Uuid(tran.params.id));
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
