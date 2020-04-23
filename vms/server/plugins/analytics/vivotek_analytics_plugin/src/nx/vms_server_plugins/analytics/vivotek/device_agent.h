#pragma once

#include <cstdint>
#include <memory>

#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/thread/cf/cfuture.h>

#include <QtCore/QString>

#include "engine.h"
#include "camera_features.h"
#include "camera_settings.h"
#include "native_metadata_source.h"
#include "timer.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    explicit DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);
    ~DeviceAgent();

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
        std::string caption, std::string description);

    cf::future<cf::unit> startMetadataStreaming();
    void stopMetadataStreaming();

    void readNextMetadata();

private:
    const std::unique_ptr<nx::network::aio::BasicPollable> m_basicPollable;
    const nx::sdk::LogUtils m_logUtils;

    const nx::utils::Url m_url; //< `http://username:password@host:port` only

    const CameraFeatures m_features;

    nx::sdk::Ptr<IHandler> m_handler;

    bool m_wantMetadata = false;
    NativeMetadataSource m_nativeMetadataSource;
    Timer m_reopenDelayer;

    // required because cf::future doesn't support detachment
    cf::future<cf::unit> m_metadataReadRetainer = cf::make_ready_future(cf::unit());
};

} // namespace nx::vms_server_plugins::analytics::vivotek
