#pragma once

#include <stdint.h>
#include <memory>
#include <optional>

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/time_helper.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/timer.h>

#include <QtCore/QString>

#include "camera_settings.h"
#include "native_metadata_source.h"
#include "object_metadata_packet_parser.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class Engine;

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    explicit DeviceAgent(Engine& engine, const nx::sdk::IDeviceInfo* deviceInfo);

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
    const nx::utils::Url m_url; //< `http://username:password@host:port` only

    bool m_isFirstDoSetSettingsCall = true;

    nx::utils::TimeHelper m_timestampAdjuster;

    nx::sdk::Ptr<IHandler> m_handler;

    NativeMetadataTypes m_neededMetadataTypes = NoNativeMetadataTypes;
    NativeMetadataTypes m_availableMetadataTypes = NoNativeMetadataTypes;
    NativeMetadataTypes m_streamedMetadataTypes = NoNativeMetadataTypes;
    std::optional<NativeMetadataSource> m_nativeMetadataSource;
    std::optional<nx::network::aio::Timer> m_timer;

    ObjectMetadataPacketParser m_objectMetadataPacketParser;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
