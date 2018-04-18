#pragma once

#include <nx_ec/ec_api.h>

namespace ec2
{

class QnTimeNotificationManager : public AbstractTimeNotificationManager
{
public:
    QnTimeNotificationManager(TimeSynchronizationManager* timeSyncManager);
    ~QnTimeNotificationManager();
private:
    QPointer<TimeSynchronizationManager> m_timeSyncManager;
};

} // namespace ec2
