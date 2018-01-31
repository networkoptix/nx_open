#pragma once

#include <nx/network/upnp/upnp_device_searcher.h>

class QnGlobalSettings;

class GlobalSettingsToDeviceSearcherSettingsAdapter:
    public nx::network::upnp::AbstractDeviceSearcherSettings
{
public:
    GlobalSettingsToDeviceSearcherSettingsAdapter(QnGlobalSettings* globalSettings);

    virtual int cacheTimeout() const override;
    virtual bool isUpnpMulticastEnabled() const override;
    virtual bool isAutoDiscoveryEnabled() const override;

private:
    QnGlobalSettings* m_globalSettings;
    const nx::network::upnp::DeviceSearcherDefaultSettings m_defaultSettings;
};
