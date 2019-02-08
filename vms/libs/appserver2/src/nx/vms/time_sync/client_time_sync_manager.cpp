#include "client_time_sync_manager.h"

#include <common/common_module.h>
#include <network/router.h>
#include <api/global_settings.h>

namespace nx {
namespace vms {
namespace time_sync {

void ClientTimeSyncManager::updateTime()
{
    auto route = commonModule()->router()->routeTo(commonModule()->remoteGUID());
    auto networkTimeSyncInterval = commonModule()->globalSettings()->syncTimeExchangePeriod();
    if (route.isValid())
    {
        if (!m_lastSyncTimeInterval.hasExpired(networkTimeSyncInterval))
            return;
        if (loadTimeFromServer(route))
            m_lastSyncTimeInterval.restart();
    }
}

} // namespace time_sync
} // namespace vms
} // namespace nx
