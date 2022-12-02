// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "videowall_notification_manager.h"

namespace ec2 {

QnVideowallNotificationManager::QnVideowallNotificationManager()
{
}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::VideowallData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveVideowall);
    emit addedOrUpdated(tran.params, source);
}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::removeVideowall);
    emit removed(tran.params.id, source);
}

void QnVideowallNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::VideowallControlMessageData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::videowallControl);
    emit controlMessage(tran.params);
}

} // namespace ec2
