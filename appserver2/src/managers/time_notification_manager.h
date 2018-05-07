#pragma once

#include <nx_ec/ec_api.h>
#include <transaction/transaction.h>

namespace nx {
namespace time_sync {
class TimeSyncManager;
}
}

namespace ec2
{

class QnTimeNotificationManager : public AbstractTimeNotificationManager
{
public:
    QnTimeNotificationManager(nx::time_sync::TimeSyncManager* timeSyncManager);
    virtual ~QnTimeNotificationManager() override {}

    void triggerNotification(
        const QnTransaction<ApiPeerSystemTimeData> &transaction,
        NotificationSource source);
};

typedef std::shared_ptr<QnTimeNotificationManager> QnTimeNotificationManagerPtr;

} // namespace ec2
