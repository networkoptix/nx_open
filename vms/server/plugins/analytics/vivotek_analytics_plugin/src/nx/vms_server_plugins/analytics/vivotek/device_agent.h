#pragma once

#include <cstdint>

#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

#include <QtCore/QString>

#include "engine.h"
#include "native_metadata_source.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>,
    nx::network::aio::BasicPollable
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
    void emitDiagnostic(
        nx::sdk::IPluginDiagnosticEvent::Level level,
        const QString& caption, const QString& description);

    void reopenNativeMetadataSource();

    void readNextMetadata();

private:
    const nx::sdk::LogUtils m_logUtils;

    nx::utils::Url m_url; //< `//username:password@host:port` only

    nx::sdk::Ptr<IHandler> m_handler;

    bool m_wantMetadata = false;
    NativeMetadataSource m_nativeMetadataSource;
    nx::network::aio::Timer m_reopenDelayer;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
