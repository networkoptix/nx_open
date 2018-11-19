#pragma once

#include <atomic>

#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>

#include <nx/utils/std/optional.h>
#include <nx/streaming/abstract_data_consumer.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/common_metadata_types.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/analytics/metadata_handler.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/analytics/device_agent_handler.h>

#include <nx/mediaserver/sdk_support/loggers.h>

namespace nx::mediaserver::analytics {

class DeviceAnalyticsBinding:
    public QnAbstractDataConsumer,
    public nx::mediaserver::ServerModuleAware

{
    using base_type = QnAbstractDataConsumer;
    using Engine = nx::sdk::analytics::Engine;
    using DeviceAgent = nx::sdk::analytics::DeviceAgent;
public:
    DeviceAnalyticsBinding(
        QnMediaServerModule* serverModule,
        QnVirtualCameraResourcePtr device,
        nx::mediaserver::resource::AnalyticsEngineResourcePtr engine);

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

    sdk_support::SharedPtr<DeviceAgent> createDeviceAgent();
    std::unique_ptr<nx::mediaserver::analytics::DeviceAgentHandler> createHandler();
    std::optional<nx::vms::api::analytics::DeviceAgentManifest> loadDeviceAgentManifest(
        const sdk_support::SharedPtr<DeviceAgent>& deviceAgent);

    bool updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);
    bool updateDescriptorsWithManifest(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest);

    sdk_support::UniquePtr<nx::sdk::analytics::CommonMetadataTypes> neededMetadataTypes() const;
    std::unique_ptr<sdk_support::AbstractManifestLogger> makeLogger(
        const QString& manifestTypes) const;

private:
    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;

    // TODO: #dmishin: Rename to m_engineResource.
    nx::mediaserver::resource::AnalyticsEngineResourcePtr m_engine;

    std::unique_ptr<nx::mediaserver::analytics::DeviceAgentHandler> m_handler;

    // TODO: #dmishin: Rename to m_deviceAgent.
    sdk_support::SharedPtr<DeviceAgent> m_sdkDeviceAgent;

    QnAbstractDataReceptorPtr m_metadataSink;

    QVariantMap m_currentSettings;
    std::atomic<bool> m_started{false};
    std::atomic<bool>m_isStreamConsumer{false};
};

} // namespace nx::mediaserver::analytics
