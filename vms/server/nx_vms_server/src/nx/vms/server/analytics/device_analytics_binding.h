#pragma once

#include <atomic>

#include <QtCore/QVariantMap>

#include <core/resource/resource_fwd.h>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>

#include <nx/sdk/helpers/ptr.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/metadata_types.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/analytics/metadata_handler.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/device_agent_handler.h>

#include <nx/vms/server/sdk_support/loggers.h>

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

    bool startAnalytics(const QVariantMap& settings);
    void stopAnalytics();
    bool restartAnalytics(const QVariantMap& settings);
    bool updateNeededMetadataTypes();

    QVariantMap getSettings() const;
    void setSettings(const QVariantMap& settings);

    void setMetadataSink(QnAbstractDataReceptorPtr dataReceptor);
    bool isStreamConsumer() const;
    std::optional<nx::vms::api::analytics::EngineManifest> engineManifest() const;

    virtual void putData(const QnAbstractDataPacketPtr& data) override;

protected:
    virtual bool processData(const QnAbstractDataPacketPtr& data) override;

private:
    bool startAnalyticsUnsafe(const QVariantMap& settings);
    void stopAnalyticsUnsafe();

    nx::sdk::Ptr<DeviceAgent> createDeviceAgent();
    std::unique_ptr<nx::vms::server::analytics::DeviceAgentHandler> createHandler();
    std::optional<nx::vms::api::analytics::DeviceAgentManifest> loadDeviceAgentManifest(
        const nx::sdk::Ptr<DeviceAgent>& deviceAgent);

    bool updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);
    bool updateDescriptorsWithManifest(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    nx::sdk::Ptr<nx::sdk::analytics::MetadataTypes> neededMetadataTypes() const;
    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger(
        const QString& manifestTypes) const;

    QVariantMap mergeWithDbAndDefaultSettings(const QVariantMap& settingsFromUser) const;

    bool setSettingsInternal(const QVariantMap& settingsFromUser);

private:
    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;

    // TODO: #dmishin: Rename to m_engineResource.
    nx::vms::server::resource::AnalyticsEngineResourcePtr m_engine;

    std::unique_ptr<nx::vms::server::analytics::DeviceAgentHandler> m_handler;

    nx::sdk::Ptr<DeviceAgent> m_deviceAgent;

    QnAbstractDataReceptorPtr m_metadataSink;

    std::atomic<bool> m_started{false};
    std::atomic<bool>m_isStreamConsumer{false};
};

} // namespace nx::vms::server::analytics
