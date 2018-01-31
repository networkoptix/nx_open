#include "global_settings_to_device_searcher_settings_adapter.h"

#include <api/global_settings.h>

static const int PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS = 24 * 60 * 60 * 1000;

GlobalSettingsToDeviceSearcherSettingsAdapter::GlobalSettingsToDeviceSearcherSettingsAdapter(
    QnGlobalSettings* globalSettings)
    :
    m_globalSettings(globalSettings)
{
}

int GlobalSettingsToDeviceSearcherSettingsAdapter::cacheTimeout() const
{
    const auto disabledVendors = m_globalSettings->disabledVendorsSet();
    if (disabledVendors.size() == 1 && disabledVendors.contains(lit("all=partial")))
        return PARTIAL_DISCOVERY_XML_DESCRIPTION_LIVE_TIME_MS;

    return m_defaultSettings.cacheTimeout();
}

bool GlobalSettingsToDeviceSearcherSettingsAdapter::isUpnpMulticastEnabled() const
{
    if (m_globalSettings->isNewSystem())
        return false;

    return
        m_globalSettings->isAutoDiscoveryEnabled() ||
        m_globalSettings->isUpnpPortMappingEnabled();
}

bool GlobalSettingsToDeviceSearcherSettingsAdapter::isAutoDiscoveryEnabled() const
{
    if (m_globalSettings->isNewSystem())
        return false;

    return m_globalSettings->isAutoDiscoveryEnabled();
}
