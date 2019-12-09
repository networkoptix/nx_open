#pragma once

#include <atomic>

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

#include <nx/vms/server/analytics/stream_requirements.h>
#include <nx/vms/server/analytics/device_agent_handler.h>
#include <nx/vms/server/analytics/i_stream_data_receptor.h>
#include <nx/vms/server/analytics/metadata_handler.h>
#include <nx/vms/server/analytics/wrappers/fwd.h>

namespace nx::vms::server::analytics {

class DeviceAnalyticsBinding:
    public QnAbstractDataConsumer,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    using base_type = QnAbstractDataConsumer;
    using Engine = nx::sdk::analytics::IEngine;
    using DeviceAgent = nx::sdk::analytics::IDeviceAgent;

public:
    DeviceAnalyticsBinding(
        QnMediaServerModule* serverModule,
        QnVirtualCameraResourcePtr device,
        nx::vms::server::resource::AnalyticsEngineResourcePtr engine);

    virtual ~DeviceAnalyticsBinding() override;

    bool startAnalytics(const QJsonObject& settings);
    void stopAnalytics();
    bool restartAnalytics(const QJsonObject& settings);
    bool updateNeededMetadataTypes();

    bool hasAliveDeviceAgent() const;

    QJsonObject getSettings() const;
    void setSettings(const QJsonObject& settings);

    void setMetadataSink(QnAbstractDataReceptorPtr dataReceptor);
    bool isStreamConsumer() const;
    std::optional<nx::vms::api::analytics::EngineManifest> engineManifest() const;

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    std::optional<StreamRequirements> streamRequirements() const;

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

private:
    bool startAnalyticsUnsafe(const QJsonObject& settings);
    void stopAnalyticsUnsafe();

    wrappers::DeviceAgentPtr createDeviceAgentUnsafe();
    nx::sdk::Ptr<nx::vms::server::analytics::DeviceAgentHandler> createHandlerUnsafe();

    bool updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);
    bool updateDescriptorsWithManifest(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    sdk_support::MetadataTypes neededMetadataTypes() const;

    void setSettingsInternal(const QJsonObject& settings);

    void logIncomingFrame(sdk::Ptr<sdk::analytics::IDataPacket> frame);

    bool updatePluginInfo() const;

    std::optional<StreamRequirements> calculateStreamRequirements();

    bool isStreamConsumerUnsafe() const;

private:
    struct DeviceAgentContext
    {
        nx::sdk::Ptr<nx::vms::server::analytics::DeviceAgentHandler> handler;
        wrappers::DeviceAgentPtr deviceAgent;
    };

    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;
    nx::vms::server::resource::AnalyticsEngineResourcePtr m_engine;
    DeviceAgentContext m_deviceAgentContext;

    QnAbstractDataReceptorPtr m_metadataSink;
    std::atomic<bool> m_started{false};
    nx::analytics::MetadataLogger m_incomingFrameLogger;
    std::optional<sdk_support::MetadataTypes> m_lastNeededMetadataTypes;
    std::optional<StreamRequirements> m_cachedStreamRequirements;
};

} // namespace nx::vms::server::analytics
