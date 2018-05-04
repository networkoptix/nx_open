#pragma once

#include <nx_ec/ec_api.h>

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
    ~QnTimeNotificationManager();
};

} // namespace ec2
