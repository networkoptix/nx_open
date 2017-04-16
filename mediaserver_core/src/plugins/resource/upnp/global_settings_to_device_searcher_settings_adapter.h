#pragma once

#include <nx/network/upnp/upnp_device_searcher.h>

class QnGlobalSettings;

class GlobalSettingsToDeviceSearcherSettingsAdapter:
    public nx_upnp::AbstractDeviceSearcherSettings
{
public:
    GlobalSettingsToDeviceSearcherSettingsAdapter(QnGlobalSettings* globalSettings);

    virtual int cacheTimeout() const override;
    virtual bool isUpnpMulticastEnabled() const override;

private:
    QnGlobalSettings* m_globalSettings;
    const nx_upnp::DeviceSearcherDefaultSettings m_defaultSettings;
};
