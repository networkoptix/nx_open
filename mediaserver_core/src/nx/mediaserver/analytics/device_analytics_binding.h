#pragma once

#include <atomic>

#include <QtCore/QVariant>

#include <core/resource/resource_fwd.h>

#include <nx/utils/std/optional.h>
#include <nx/streaming/abstract_data_consumer.h>

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/analytics/metadata_handler.h>

namespace nx::mediaserver::analytics {

class DeviceAnalyticsBinding: public QnAbstractDataConsumer
{
    using base_type = QnAbstractDataConsumer;
    using Engine = nx::sdk::analytics::Engine;
    using DeviceAgent = nx::sdk::analytics::DeviceAgent;
public:
    DeviceAnalyticsBinding(
        QnVirtualCameraResourcePtr device,
        nx::mediaserver::resource::AnalyticsEngineResourcePtr engine);

    virtual ~DeviceAnalyticsBinding() override;

    bool startAnalytics(const QVariantMap& settings);
    void stopAnalytics();
    bool restartAnalytics(const QVariantMap& settings);

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

private:
    sdk_support::SharedPtr<DeviceAgent> createDeviceAgent();
    std::shared_ptr<MetadataHandler> createMetadataHandler();
    std::optional<nx::vms::api::analytics::DeviceAgentManifest> loadManifest(
        const sdk_support::SharedPtr<DeviceAgent>& deviceAgent);

    void updateDeviceWithManifest(const nx::vms::api::analytics::DeviceAgentManifest& manifest);

private:
    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;
    nx::mediaserver::resource::AnalyticsEngineResourcePtr m_engine;

    sdk_support::SharedPtr<DeviceAgent> m_sdkDeviceAgent;
    std::shared_ptr<MetadataHandler> m_metadataHandler;
    QnAbstractDataReceptorPtr m_metadataSink;

    QVariantMap m_currentSettings;
    std::atomic<bool> m_started{false};
    std::atomic<bool>m_isStreamConsumer{false};
};

} // namespace nx::mediaserver::analytics
