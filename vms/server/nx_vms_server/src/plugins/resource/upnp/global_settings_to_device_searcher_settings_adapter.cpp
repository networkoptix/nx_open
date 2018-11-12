#include "global_settings_to_device_searcher_settings_adapter.h"

#include <core/resource_management/resource_discovery_manager.h>

GlobalSettingsToDeviceSearcherSettingsAdapter::GlobalSettingsToDeviceSearcherSettingsAdapter(
    QnResourceDiscoveryManager* discoveryManager)
    :
    m_discoveryManager(discoveryManager)
{
}

int GlobalSettingsToDeviceSearcherSettingsAdapter::cacheTimeout() const
{
    static const int kBigCacheTimeoutMs = 24 * 60 * 60 * 1000;
    if (m_discoveryManager->discoveryMode() != DiscoveryMode::fullyEnabled)
        return kBigCacheTimeoutMs;

    return m_defaultSettings.cacheTimeout();
}
