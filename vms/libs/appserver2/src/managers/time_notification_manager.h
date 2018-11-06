#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

#include <nx/vms/time_sync/abstract_time_sync_manager.h>

namespace ec2 {

class QnTimeNotificationManager: public AbstractTimeNotificationManager
{
public:
    QnTimeNotificationManager(nx::vms::time_sync::AbstractTimeSyncManager* timeSyncManager);
    virtual ~QnTimeNotificationManager() override {}

    void triggerNotification(
        const QnTransaction<nx::vms::api::PeerSyncTimeData> &transaction,
        NotificationSource source);
};

using QnTimeNotificationManagerPtr = std::shared_ptr<QnTimeNotificationManager> ;

} // namespace ec2
