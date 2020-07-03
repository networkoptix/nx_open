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
#include <nx/network/aio/timer.h>

#include <QtCore/QString>

#include "engine.h"
#include "camera_settings.h"
#include "native_metadata_source.h"
#include "event_prolonger.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    explicit DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo);

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
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

    void updateAvailableMetadataTypes(const CameraSettings& settings);

    void refreshMetadataStreaming();

    void startMetadataStreaming();
    void stopMetadataStreaming();

    void streamMetadataPackets();
    void streamProlongedEventMetadataPackets();

private:
    const std::unique_ptr<nx::network::aio::BasicPollable> m_basicPollable;
    const nx::sdk::LogUtils m_logUtils;
    const nx::utils::Url m_url; //< `http://username:password@host:port` only

    nx::sdk::Ptr<IHandler> m_handler;

    NativeMetadataTypes m_neededMetadataTypes = NoNativeMetadataTypes;
    NativeMetadataTypes m_availableMetadataTypes = NoNativeMetadataTypes;
    NativeMetadataTypes m_streamedMetadataTypes = NoNativeMetadataTypes;
    std::optional<NativeMetadataSource> m_nativeMetadataSource;
    std::optional<nx::network::aio::Timer> m_timer;
    std::optional<EventProlonger> m_eventProlonger;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
