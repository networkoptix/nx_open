#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <map>
#include <memory>

#include <boost/optional/optional.hpp>

#include "resource_analytics_context.h"

#include <nx/utils/log/log.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>
#include <nx/mediaserver/analytics/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/video_data_packet.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/sdk/analytics/uncompressed_video_frame.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/plugins/settings.h>

#include <nx/mediaserver/analytics/device_analytics_context.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx {
namespace mediaserver {
namespace analytics {

class MetadataHandler;

class Manager final:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    using ResourceAnalyticsContextMap = std::map<QnUuid, ResourceAnalyticsContext>;

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

    /**
     * @return An object that will receive the data from a video data provider.
     */
    QWeakPointer<VideoDataReceptor> createVideoDataReceptor(const QnUuid& id);

    /**
     * Register data receptor that will receive metadata packets.
     */
    void registerDataReceptor(
        const QnResourcePtr& resource,
        QWeakPointer<QnAbstractDataReceptor> dataReceptor);

    using EngineList = QList<nx::sdk::analytics::Engine*>;

    /**
     * @return Deserialized Engine manifest, or none on error.
     */
    boost::optional<nx::vms::api::analytics::EngineManifest> loadEngineManifest(
        nx::sdk::analytics::Engine* engine);

public slots:
    void initExistingResources();

private:
    using PixelFormat = nx::sdk::analytics::UncompressedVideoFrame::PixelFormat;

    std::unique_ptr<const nx::plugins::SettingsHolder> loadSettingsFromFile(
        const QString& fileDescription, const QString& filename);

    void setDeviceAgentSettings(
        nx::sdk::analytics::DeviceAgent* deviceAgent,
        const QnVirtualCameraResourcePtr& camera);

    void setEngineSettings(nx::sdk::analytics::Engine* engine);

    void createDeviceAgentsForResourceUnsafe(const QnVirtualCameraResourcePtr& camera);

    nx::sdk::analytics::DeviceAgent* createDeviceAgent(
        const QnVirtualCameraResourcePtr& camera,
        nx::sdk::analytics::Engine* engine) const;

    void releaseDeviceAgentsUnsafe(const QnVirtualCameraResourcePtr& camera);

    void releaseDeviceAgentsUnsafe(const QnUuid& resourceId);

    MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void handleResourceChanges(const QnResourcePtr& resource);
    void updateOnlineDeviceUnsafe(const QnVirtualCameraResourcePtr& device);
    void updateOfflineDeviceUnsafe(const QnVirtualCameraResourcePtr& device);

    bool isDeviceAlive(const QnVirtualCameraResourcePtr& camera) const;

    void startFetchingMetadataForDeviceUnsafe(
        const QnUuid& resourceId,
        ResourceAnalyticsContext& context,
        const QSet<QString>& eventTypeIds);

    template<typename T>
    boost::optional<T> deserializeManifest(const char* manifestString) const
    {
        bool success = false;
        auto deserialized = QJson::deserialized<T>(
            QString(manifestString).toUtf8(),
            T(),
            &success);

        if (!success)
        {
            NX_ERROR(this) << "Unable to deserialize analytics plugin manifest:" << manifestString;
            return boost::none;
        }

        return deserialized;
    }

    void assignEngineManifestToServer(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QnMediaServerResourcePtr& server);

    nx::vms::api::analytics::EngineManifest mergeEngineManifestToServer(
        const nx::vms::api::analytics::EngineManifest& manifest,
        const QnMediaServerResourcePtr& server);

    void addManifestToCamera(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest,
        const QnVirtualCameraResourcePtr& camera);

    void putVideoFrame(
        const QnUuid& id,
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame);

    boost::optional<PixelFormat> pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest);

    void issueMissingUncompressedFrameWarningOnce();

    void updateDeviceAnalytics(const QnVirtualCameraResourcePtr& device);




    std::shared_ptr<DeviceAnalyticsContext> context(const QnUuid& deviceId) const;
    std::shared_ptr<DeviceAnalyticsContext> context(const QnVirtualCameraResourcePtr& device) const;

    bool isLocalDevice(const QnVirtualCameraResourcePtr& device) const;

    void at_deviceAdded(const QnVirtualCameraResourcePtr& device);
    void at_deviceRemoved(const QnVirtualCameraResourcePtr& device);
    void at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device);
    void at_devicePropertyChanged(
        const QnVirtualCameraResourcePtr& device,
        const QString& propertyName);

    void handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device);
    void handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device);

    void at_engineAdded(const nx::vms::common::AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

    void registerMetadataSink(
        const QnResourcePtr& device,
        QWeakPointer<QnAbstractDataReceptor> metadataSink);

    QWeakPointer<VideoDataReceptor> registerMediaSource(const QnUuid& deviceId);

    QWeakPointer<QnAbstractDataReceptor> metadataSink(const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<QnAbstractDataReceptor> metadataSink(const QnUuid& device) const;
    QWeakPointer<VideoDataReceptor> mediaSource(const QnVirtualCameraResourcePtr& device) const;
    QWeakPointer<VideoDataReceptor> mediaSource(const QnUuid& device) const;

private:
    ResourceAnalyticsContextMap m_contexts;
    QnMutex m_contextMutex;
    bool m_uncompressedFrameWarningIssued = false;
    nx::debugging::VisualMetadataDebuggerPtr m_visualMetadataDebugger;
    QThread* m_thread;


    std::map<QnUuid, std::shared_ptr<DeviceAnalyticsContext>> m_deviceAnalyticsContexts;
    std::map<QnUuid, QWeakPointer<QnAbstractDataReceptor>> m_metadataSinks;
    std::map<QnUuid, QSharedPointer<VideoDataReceptor>> m_mediaSources;
};

} // namespace analytics
} // namespace mediaserver
} // namespace nx

