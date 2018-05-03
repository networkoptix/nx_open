#include "time_notification_manager.h"
#include "time_manager.h"

namespace ec2 {

QnTimeNotificationManager::QnTimeNotificationManager(
    TimeSynchronizationManager* timeSyncManager):
    m_timeSyncManager(timeSyncManager)
{
    connect(timeSyncManager, &TimeSynchronizationManager::timeChanged,
        this, &QnTimeNotificationManager::timeChanged,
        Qt::DirectConnection);
}

QnTimeNotificationManager::~QnTimeNotificationManager()
{
    //safely disconnecting from TimeSynchronizationManager
    if (m_timeSyncManager)
        m_timeSyncManager->disconnectAndJoin(this);
}

} // namespace ec2
