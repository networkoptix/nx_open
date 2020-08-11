#pragma once

#include <atomic>
#include <memory>

#include <QtCore/QJsonObject>

#include <core/resource/resource_fwd.h>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>

#include <nx/analytics/metadata_logger.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/server/sdk_support/types.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/server/analytics/types.h>
#include <nx/vms/server/analytics/settings.h>
#include <nx/vms/server/analytics/stream_requirements.h>
#include <nx/vms/server/analytics/device_agent_handler.h>
#include <nx/vms/server/analytics/stream_data_receptor.h>
#include <nx/vms/server/analytics/metadata_handler.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>

namespace nx::vms::server::analytics {

class DeviceAnalyticsBinding:
    public std::enable_shared_from_this<DeviceAnalyticsBinding>,
    public QnAbstractDataConsumer,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    using base_type = QnAbstractDataConsumer;
    using Engine = nx::sdk::analytics::IEngine;
    using DeviceAgent = nx::sdk::analytics::IDeviceAgent;

public:
    DeviceAnalyticsBinding(
        QnMediaServerModule* serverModule,
        nx::vms::server::resource::CameraPtr device,
        nx::vms::server::resource::AnalyticsEngineResourcePtr engine);

    virtual ~DeviceAnalyticsBinding() override;

    bool canAcceptData() const override;

    void setMaxQueueDuration(std::chrono::microseconds value);

    bool startAnalytics();
    void stopAnalytics();
    bool restartAnalytics();
    bool updateNeededMetadataTypes();

    bool hasAliveDeviceAgent() const;

    SettingsResponse getSettings() const;
    SettingsResponse setSettings(const SetSettingsRequest& settings);

    void setMetadataSinks(MetadataSinkSet metadataSinks);
    bool isStreamConsumer() const;
    std::optional<nx::vms::api::analytics::EngineManifest> engineManifest() const;

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    std::optional<StreamRequirements> streamRequirements() const;

    void recalculateStreamRequirements();

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

private:
    bool startAnalyticsUnsafe();
    void stopAnalyticsUnsafe();

    wrappers::DeviceAgentPtr createDeviceAgentUnsafe();
    nx::sdk::Ptr<nx::vms::server::analytics::DeviceAgentHandler> createHandlerUnsafe();

    bool updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);
    bool updateDescriptorsWithManifest(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    sdk_support::MetadataTypes neededMetadataTypes() const;

    SettingsResponse setSettingsInternal(
        const SetSettingsRequest& settingsRequest,
        bool isInitialSettings);

    void logIncomingDataPacket(sdk::Ptr<sdk::analytics::IDataPacket> frame);

    bool updatePluginInfo() const;

    std::optional<StreamRequirements> calculateStreamRequirements();

    bool isStreamConsumerUnsafe() const;

    void notifySettingsMaybeChanged() const;

    void initializeSettingsContext() const;

    SettingsContext updateSettingsContext(
        const api::analytics::SettingsValues& requestValues,
        const sdk_support::SdkSettingsResponse& sdkSettingsResponse) const;

    api::analytics::SettingsValues prepareSettings(
        const SettingsContext& settingsContext,
        const api::analytics::SettingsValues& settings) const;

    SettingsResponse prepareSettingsResponse(
        const SettingsContext& settingsContext,
        const sdk_support::SdkSettingsResponse& sdkSettingsResponse) const;

private:
    struct DeviceAgentContext
    {
        nx::sdk::Ptr<nx::vms::server::analytics::DeviceAgentHandler> handler;
        wrappers::DeviceAgentPtr deviceAgent;

        mutable SettingsContext settingsContext;
    };

    mutable nx::Mutex m_mutex;
    nx::vms::server::resource::CameraPtr m_device;
    nx::vms::server::resource::AnalyticsEngineResourcePtr m_engine;
    DeviceAgentContext m_deviceAgentContext;

    MetadataSinkSet m_metadataSinks;
    std::atomic<bool> m_started{false};
    nx::analytics::MetadataLogger m_incomingFrameLogger;
    nx::analytics::MetadataLogger m_incomingInbandMetadataLogger;
    std::optional<sdk_support::MetadataTypes> m_lastNeededMetadataTypes;
    std::optional<StreamRequirements> m_cachedStreamRequirements;
    std::chrono::microseconds m_maxQueueDuration{};
};

} // namespace nx::vms::server::analytics
