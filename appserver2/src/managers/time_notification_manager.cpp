#include "time_notification_manager.h"
#include <nx/vms/time_sync/time_sync_manager.h>

namespace ec2 {

QnTimeNotificationManager::QnTimeNotificationManager(nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager)
{
    connect(
        timeSyncManager, &nx::vms::time_sync::TimeSyncManager::timeChanged,
        this, &QnTimeNotificationManager::timeChanged);
}

void QnTimeNotificationManager::triggerNotification(
    const QnTransaction<nx::vms::api::PeerSyncTimeData>& /*transaction*/,
    NotificationSource /*source*/)
{
    emit primaryTimeServerTimeChanged();
}

} // namespace ec2
