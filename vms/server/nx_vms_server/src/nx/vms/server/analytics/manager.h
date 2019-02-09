#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QThread>

#include <utils/common/connective.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/vms/server/analytics/device_analytics_context.h>
#include <nx/vms/server/analytics/proxy_video_data_receptor.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/rule_holder.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/json.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx::vms::server::analytics {

class Manager final:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware
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
        const QnResourcePtr& deviceResource,
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
    using AnalyticsEngineResourcePtr = nx::vms::server::resource::AnalyticsEngineResourcePtr;

    QSharedPointer<DeviceAnalyticsContext> context(const QnUuid& deviceId) const;
    QSharedPointer<DeviceAnalyticsContext> context(const QnVirtualCameraResourcePtr& device) const;

    void at_deviceAdded(const QnVirtualCameraResourcePtr& device);
    void at_deviceRemoved(const QnVirtualCameraResourcePtr& device);
    void at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device);

    void at_deviceUserEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& device);

    void at_deviceStatusChanged(const QnResourcePtr& deviceResource);

    void handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device);
    void handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device);

    void at_engineAdded(const AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(
        const AnalyticsEngineResourcePtr& engine,
        const QString& propertyName);

    void at_engineInitializationStateChanged(const AnalyticsEngineResourcePtr& engine);

    QWeakPointer<QnAbstractDataReceptor> metadataSink(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<QnAbstractDataReceptor> metadataSink(const QnUuid& deviceId) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(const QnUuid& deviceId) const;

    nx::vms::server::resource::AnalyticsEngineResourceList localEngines() const;
    QnVirtualCameraResourceList localDevices() const;
    bool isLocalDevice(const QnVirtualCameraResourcePtr& device) const;

    std::set<QnUuid> compatibleEngineIds(const QnVirtualCameraResourcePtr& device) const;

    void updateCompatibilityWithEngines(const QnVirtualCameraResourcePtr& device);
    void updateCompatibilityWithDevices(const AnalyticsEngineResourcePtr& engine);
    void updateEnabledAnalyticsEngines(const QnVirtualCameraResourcePtr& device);

    void removeDeviceDescriptor(const QnVirtualCameraResourcePtr& device) const;
    void removeEngineFromCompatible(const AnalyticsEngineResourcePtr& engine) const;

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
