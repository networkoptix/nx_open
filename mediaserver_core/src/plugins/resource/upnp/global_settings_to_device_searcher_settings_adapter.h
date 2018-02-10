#pragma once

#include <nx/network/upnp/upnp_device_searcher.h>

class QnResourceDiscoveryManager;

class GlobalSettingsToDeviceSearcherSettingsAdapter:
    public nx_upnp::AbstractDeviceSearcherSettings
{
public:
    GlobalSettingsToDeviceSearcherSettingsAdapter(QnResourceDiscoveryManager* commonModule);

    virtual int cacheTimeout() const override;

private:
    QnResourceDiscoveryManager* m_discoveryManager;
    const nx_upnp::DeviceSearcherDefaultSettings m_defaultSettings;
};
