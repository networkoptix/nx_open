// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_sync_manager.h"

#include <network/router.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx_ec/managers/abstract_time_manager.h>

using namespace nx::vms::common;

namespace nx::vms::client::core {

TimeSyncManager::TimeSyncManager(SystemContext* systemContext, const QnUuid& serverId):
    base_type(systemContext),
    m_serverId(serverId)
{
    connect(systemSettings(),
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
    updateTime();
}

void TimeSyncManager::updateTime()
{
    auto route = QnRouter::routeTo(m_serverId, systemContext());
    auto networkTimeSyncInterval = systemSettings()->syncTimeExchangePeriod();
    if (route.isValid())
    {
        if (!m_lastSyncTimeInterval.hasExpired(networkTimeSyncInterval))
            return;

        const auto result = loadTimeFromServer(route, /*checkTimeSource*/ false);
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
