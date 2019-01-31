#pragma once

#include <nx/vms/time_sync/time_sync_manager.h>
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace vms {
namespace time_sync {

class ClientTimeSyncManager: public TimeSyncManager
{
    Q_OBJECT
public:
    using TimeSyncManager::TimeSyncManager;
    virtual ~ClientTimeSyncManager() override { stop(); }
protected:
    virtual void updateTime() override;
private:
    nx::utils::ElapsedTimer m_lastSyncTimeInterval;
};

} // namespace time_sync
} // namespace vms
} // namespace nx
