// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_notification_manager.h"

namespace ec2 {

void ShowreelNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeShowreel);
    emit removed(nx::Uuid(tran.params.id));
}

void ShowreelNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::ShowreelData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveShowreel);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2
