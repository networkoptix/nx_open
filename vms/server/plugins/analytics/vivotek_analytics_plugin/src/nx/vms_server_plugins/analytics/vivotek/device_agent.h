#pragma once

#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>

#include "engine.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    explicit DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
        const nx::sdk::IStringMap* settings) override;

    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;

    virtual void setHandler(IHandler* handler) override;

    virtual void doSetNeededMetadataTypes(nx::sdk::Result<void>* outResult,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

private:
    const nx::sdk::LogUtils m_logUtils;

    nx::sdk::Ptr<const nx::sdk::IDeviceInfo> m_deviceInfo;
    nx::sdk::Ptr<IHandler> m_handler;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
