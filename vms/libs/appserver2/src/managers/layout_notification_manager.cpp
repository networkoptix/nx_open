// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_notification_manager.h"

namespace ec2 {

void QnLayoutNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::removeLayout);
    emit removed(nx::Uuid(tran.params.id), source);
}

void QnLayoutNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::LayoutData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayout);
    emit addedOrUpdated(tran.params, source);
}

void QnLayoutNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::LayoutDataList>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayouts);
    for (const auto& layout: tran.params)
        emit addedOrUpdated(layout, source);
}

} // namespace ec2
