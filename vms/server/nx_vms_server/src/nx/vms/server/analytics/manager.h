#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <map>
#include <memory>

#include <utils/common/connective.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/vms/server/analytics/device_analytics_context.h>
#include <nx/vms/server/analytics/proxy_video_data_receptor.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/rule_holder.h>

#include <nx/utils/log/log.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/fusion/serialization/json.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx::vms::server::analytics {

class Manager final:
    public Connective<QObject>,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    Manager(QnMediaServerModule* serverModule);
    ~Manager();

    void init();
    void stop();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

    void at_resourceParentIdChanged(const QnResourcePtr& resource);
    void at_resourcePropertyChanged(const QnResourcePtr& resource, const QString& propertyName);

    void registerMetadataSink(
        const QnResourcePtr& device,
        QWeakPointer<QnAbstractDataReceptor> metadataSink);

    QWeakPointer<AbstractVideoDataReceptor> registerMediaSource(const QnUuid& deviceId);

    void setSettings(
        const QString& deviceId,
        const QString& engineId,
        const QVariantMap& deviceAgentSettings);

    QVariantMap getSettings(const QString& deviceId, const QString& engineId) const;

    void setSettings(const QString& engineId, const QVariantMap& engineSettings);
    QVariantMap getSettings(const QString& engineId) const;

public slots:
    void initExistingResources();

private:
    QSharedPointer<DeviceAnalyticsContext> context(const QnUuid& deviceId) const;
    QSharedPointer<DeviceAnalyticsContext> context(const QnVirtualCameraResourcePtr& device) const;

    bool isLocalDevice(const QnVirtualCameraResourcePtr& device) const;

    void at_deviceAdded(const QnVirtualCameraResourcePtr& device);
    void at_deviceRemoved(const QnVirtualCameraResourcePtr& device);
    void at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device);
    void at_devicePropertyChanged(
        const QnVirtualCameraResourcePtr& device,
        const QString& propertyName);

    void at_deviceStatusChanged(const QnResourcePtr& device);

    void handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device);
    void handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device);

    void at_engineAdded(const resource::AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const resource::AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(
        const resource::AnalyticsEngineResourcePtr& engine,
        const QString& propertyName);

    void at_engineInitializationStateChanged(
        const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

    QWeakPointer<QnAbstractDataReceptor> metadataSink(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<QnAbstractDataReceptor> metadataSink(const QnUuid& device) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(const QnUuid& device) const;

    nx::vms::server::resource::AnalyticsEngineResourceList localEngines() const;
    std::set<QnUuid> compatibleEngineIds(const QnVirtualCameraResourcePtr& device) const;

    void updateCompatibilityWithEngines(const QnVirtualCameraResourcePtr& device);
    void updateCompatibilityWithDevices(
        const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

    void removeDeviceDescriptor(const QnVirtualCameraResourcePtr& device);
    void removeEngineFromCompatible(
        const nx::vms::server::resource::AnalyticsEngineResourcePtr& engine);

private:
    mutable QnMutex m_contextMutex;
    QThread* m_thread;
    nx::debugging::VisualMetadataDebuggerPtr m_visualMetadataDebugger;

    std::map<QnUuid, QSharedPointer<DeviceAnalyticsContext>> m_deviceAnalyticsContexts;

    // TODO: Switch to std pointers.
    std::map<QnUuid, QWeakPointer<QnAbstractDataReceptor>> m_metadataSinks;
    std::map<QnUuid, QSharedPointer<ProxyVideoDataReceptor>> m_mediaSources;
};

} // namespace nx::vms::server::analytics

