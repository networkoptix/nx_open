#pragma once

#include <vector>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_metadata_types.h>

#include "events.h"
#include "metadata_monitor.h"

namespace nx::vms_server_plugins::analytics::dahua {

class Engine;

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
private:
    class ManifestBuilder;

public:
    explicit DeviceAgent(Engine* engine, nx::sdk::Ptr<const nx::sdk::IDeviceInfo> info);
    virtual ~DeviceAgent();

    Engine* engine() const;
    nx::sdk::Ptr<const nx::sdk::IDeviceInfo> info() const;
    nx::sdk::Ptr<IHandler> handler() const;

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
        const nx::sdk::IStringMap* values) override;

    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;

    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;

    virtual void setHandler(IHandler* handler) override;

    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outResult,
        const nx::sdk::analytics::IMetadataTypes* types) override;

private:
    std::vector<const EventType*> fetchSupportedEventTypes() const;

private:
    Engine* const m_engine;
    const nx::sdk::Ptr<const nx::sdk::IDeviceInfo> m_info;
    nx::sdk::Ptr<IHandler> m_handler;
    MetadataMonitor m_metadataMonitor;
};

} // namespace nx::vms_server_plugins::analytics::dahua
