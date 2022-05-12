// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_sync_manager.h"

#include <common/common_module.h>
#include <network/router.h>
#include <nx_ec/managers/abstract_time_manager.h>
#include <nx/vms/common/system_settings.h>

using namespace nx::vms::common;

namespace nx::vms::client::core {

TimeSyncManager::TimeSyncManager(QnCommonModule* commonModule, const QnUuid& serverId):
    base_type(commonModule),
    m_serverId(serverId)
{
    connect(globalSettings(),
        &SystemSettings::timeSynchronizationSettingsChanged,
        this,
        &TimeSyncManager::resync);
}

TimeSyncManager::~TimeSyncManager()
{
    stop();
}

void TimeSyncManager::resync()
{
    m_lastSyncTimeInterval.invalidate();
}

void TimeSyncManager::updateTime()
{
    auto route = commonModule()->router()->routeTo(m_serverId, commonModule()->systemContext());
    auto networkTimeSyncInterval = globalSettings()->syncTimeExchangePeriod();
    if (route.isValid())
    {
        if (!m_lastSyncTimeInterval.hasExpired(networkTimeSyncInterval))
            return;

        const auto result = loadTimeFromServer(route);
        switch (result)
        {
            case Result::ok:
                m_lastSyncTimeInterval.restart();
                break;
            case Result::incompatibleServer:
                stop();
                break;
            default:
                break;
        }
    }
}

} // namespace nx::vms::client::core
