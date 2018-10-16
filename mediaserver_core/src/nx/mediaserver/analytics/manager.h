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

#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>

#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/mediaserver/analytics/device_analytics_context.h>
#include <nx/mediaserver/analytics/proxy_video_data_receptor.h>

#include <nx/mediaserver/resource/resource_fwd.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/analytics/rule_holder.h>

#include <nx/utils/log/log.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/fusion/serialization/json.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>

#include <nx/plugins/settings.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx::mediaserver::analytics {

class MetadataHandler;

class Manager final:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    Q_OBJECT
public:
    Manager(QnMediaServerModule* serverModule);
    ~Manager();

    void init();
    void stop();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

    void at_resourceParentIdChanged(const QnResourcePtr& resource);
    void at_resourcePropertyChanged(const QnResourcePtr& resource, const QString& propertyName);


    void registerMetadataSink(
        const QnResourcePtr& device,
        QWeakPointer<QnAbstractDataReceptor> metadataSink);

    QWeakPointer<AbstractVideoDataReceptor> registerMediaSource(const QnUuid& deviceId);

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

    void handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device);
    void handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device);

    void at_engineAdded(const resource::AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const resource::AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(
        const resource::AnalyticsEngineResourcePtr& engine,
        const QString& propertyName);

    void updateEngineSettings(const resource::AnalyticsEngineResourcePtr& engine);

    QWeakPointer<QnAbstractDataReceptor> metadataSink(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<QnAbstractDataReceptor> metadataSink(const QnUuid& device) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(
        const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<ProxyVideoDataReceptor> mediaSource(const QnUuid& device) const;

private:
    QnMutex m_contextMutex;
    QThread* m_thread;
    nx::debugging::VisualMetadataDebuggerPtr m_visualMetadataDebugger;

    std::map<QnUuid, QSharedPointer<DeviceAnalyticsContext>> m_deviceAnalyticsContexts;

    // TODO: switch to std pointers.
    std::map<QnUuid, QWeakPointer<QnAbstractDataReceptor>> m_metadataSinks;
    std::map<QnUuid, QSharedPointer<ProxyVideoDataReceptor>> m_mediaSources;
};

} // namespace nx::mediaserver::analytics

