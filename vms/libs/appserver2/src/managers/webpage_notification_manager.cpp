// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "webpage_notification_manager.h"

namespace ec2 {

QnWebPageNotificationManager::QnWebPageNotificationManager()
{
}

void QnWebPageNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::WebPageData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveWebPage);
    emit addedOrUpdated(tran.params, source);
}

void QnWebPageNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::removeWebPage);
    emit removed(nx::Uuid(tran.params.id), source);
}

} // namespace ec2
