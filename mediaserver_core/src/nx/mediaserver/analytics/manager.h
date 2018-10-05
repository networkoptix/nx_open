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
    void at_propertyChanged(const QnResourcePtr& resource, const QString& name);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

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
     * @return List of available Engines; for each item, queryInterface() is called which increases
     * the reference counter, thus, it is usually needed to release each Engine in the caller.
     */
    EngineList availableEngines() const;

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

    void saveManifestToFile(
        const char* manifest,
        const QString& fileDescription,
        const QString& pluginLibName,
        const QString& filenameExtraSuffix = "");

    void setDeviceAgentDeclaredSettings(
        nx::sdk::analytics::DeviceAgent* deviceAgent,
        const QnSecurityCamResourcePtr& camera,
        const QString& pluginLibName);

    void setEngineDeclaredSettings(
        nx::sdk::analytics::Engine* engine,
        const QString& pluginLibName);

    void createDeviceAgentsForResourceUnsafe(const QnSecurityCamResourcePtr& camera);

    nx::sdk::analytics::DeviceAgent* createDeviceAgent(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::analytics::Engine* engine) const;

    void releaseResourceDeviceAgentsUnsafe(const QnSecurityCamResourcePtr& camera);

    void releaseResourceDeviceAgentsUnsafe(const QnUuid& resourceId);

    MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const nx::vms::api::analytics::EngineManifest& manifest);

    void handleResourceChanges(const QnResourcePtr& resource);

    bool isCameraAlive(const QnSecurityCamResourcePtr& camera) const;

    void fetchMetadataForResourceUnsafe(
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

    std::pair<
        boost::optional<nx::vms::api::analytics::DeviceAgentManifest>,
        boost::optional<nx::vms::api::analytics::EngineManifest>
    >
    loadDeviceAgentManifest(
        const nx::sdk::analytics::Engine* engine,
        nx::sdk::analytics::DeviceAgent* deviceAgent,
        const QnSecurityCamResourcePtr& camera);

    void addManifestToCamera(
        const nx::vms::api::analytics::DeviceAgentManifest& manifest,
        const QnSecurityCamResourcePtr& camera);

    bool cameraInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::CameraInfo* outCameraInfo) const;

    void putVideoFrame(
        const QnUuid& id,
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame);

    boost::optional<PixelFormat> pixelFormatFromEngineManifest(
        const nx::vms::api::analytics::EngineManifest& manifest);

    void issueMissingUncompressedFrameWarningOnce();

private:
    ResourceAnalyticsContextMap m_contexts;
    QnMutex m_contextMutex;
    bool m_uncompressedFrameWarningIssued = false;
    nx::debugging::VisualMetadataDebuggerPtr m_visualMetadataDebugger;
    QThread* m_thread;
};

} // namespace analytics
} // namespace mediaserver
} // namespace nx

namespace nx {
namespace sdk {

QString toString(const nx::sdk::CameraInfo& cameraInfo);

} // namespace sdk
} // namespace nx
