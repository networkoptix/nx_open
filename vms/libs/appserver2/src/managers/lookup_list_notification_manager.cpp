// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_notification_manager.h"

namespace ec2 {

void LookupListNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::IdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeLookupList);
    emit removed(nx::Uuid(tran.params.id));
}

void LookupListNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::LookupListData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLookupList);
    emit addedOrUpdated(tran.params, source);
}

} // namespace ec2
