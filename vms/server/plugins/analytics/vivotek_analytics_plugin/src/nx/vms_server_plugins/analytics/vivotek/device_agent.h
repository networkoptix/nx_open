#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/basic_pollable.h>

#include <QtCore/QString>

#include "engine.h"
#include "camera_features.h"
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
        const nx::sdk::IStringMap* values) override;

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

    cf::future<cf::unit> startMetadataStreaming();
    cf::future<cf::unit> restartMetadataStreamingLater();
    void stopMetadataStreaming();

    cf::future<cf::unit> streamMetadataPackets();

private:
    const std::unique_ptr<nx::network::aio::BasicPollable> m_basicPollable;
    const nx::sdk::LogUtils m_logUtils;
    const nx::utils::Url m_url; //< `http://username:password@host:port` only
    const CameraFeatures m_features;

    nx::sdk::Ptr<IHandler> m_handler;

    bool m_wantMetadata = false;
    std::optional<NativeMetadataSource> m_nativeMetadataSource;
    std::optional<Timer> m_restartDelayer;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
