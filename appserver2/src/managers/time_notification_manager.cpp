#include "time_notification_manager.h"
#include <nx/time_sync/time_sync_manager.h>

namespace ec2 {

QnTimeNotificationManager::QnTimeNotificationManager(nx::time_sync::TimeSyncManager* timeSyncManager)
{
    connect(
        timeSyncManager, &nx::time_sync::TimeSyncManager::timeChanged,
        this, &QnTimeNotificationManager::timeChanged);
}

QnTimeNotificationManager::~QnTimeNotificationManager()
{
}

} // namespace ec2
