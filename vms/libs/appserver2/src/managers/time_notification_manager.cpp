// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_notification_manager.h"

namespace ec2 {

void QnTimeNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::PeerSyncTimeData>& /*transaction*/,
    NotificationSource /*source*/)
{
    emit primaryTimeServerTimeChanged();
}

} // namespace ec2
