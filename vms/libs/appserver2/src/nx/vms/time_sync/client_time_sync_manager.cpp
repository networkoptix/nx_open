#include "client_time_sync_manager.h"

#include <common/common_module.h>
#include <network/router.h>
#include <api/global_settings.h>
#include <api/common_message_processor.h>

namespace nx {
namespace vms {
namespace time_sync {

ClientTimeSyncManager::ClientTimeSyncManager(QnCommonModule* commonModule): base_type(commonModule)
{
    connect(
        commonModule->globalSettings(),
        &QnGlobalSettings::timeSynchronizationSettingsChanged,
        this,
        &ClientTimeSyncManager::forceUpdate);
}

void ClientTimeSyncManager::forceUpdate()
{
    m_lastSyncTimeInterval.invalidate();
}

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
