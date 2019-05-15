#pragma once

#include <nx/utils/elapsed_timer.h>
#include <nx/vms/time_sync/time_sync_manager.h>
#include <nx_ec/ec_api_fwd.h>

namespace nx {
namespace vms {
namespace time_sync {

class ClientTimeSyncManager: public TimeSyncManager
{
    Q_OBJECT
    using base_type = TimeSyncManager;
public:
    ClientTimeSyncManager(QnCommonModule* commonModule);
    virtual ~ClientTimeSyncManager() override { stop(); }

    virtual void stop() override;
    virtual void start() override;
    void forceUpdate();

protected:
    virtual void updateTime() override;

private:
    nx::utils::ElapsedTimer m_lastSyncTimeInterval;
    ec2::AbstractECConnectionPtr m_connection;
};

} // namespace time_sync
} // namespace vms
} // namespace nx
