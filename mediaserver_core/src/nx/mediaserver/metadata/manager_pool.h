#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <map>
#include <memory>

#include <boost/optional/optional.hpp>

#include "resource_metadata_context.h"

#include <nx/utils/log/log.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/vms/api/analytics/plugin_manifest.h>
#include <nx/vms/api/analytics/camera_manager_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/streaming/video_data_packet.h>
#include <decoders/video/ffmpeg_video_decoder.h>
#include <nx/debugging/abstract_visual_metadata_debugger.h>
#include <nx/sdk/metadata/uncompressed_video_frame.h>
#include <nx/mediaserver/server_module_aware.h>
#include <nx/plugins/settings.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx {
namespace mediaserver {
namespace metadata {

class MetadataHandler;

class ManagerPool final:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware
{
    using ResourceMetadataContextMap = std::map<QnUuid, ResourceMetadataContext>;

    Q_OBJECT
public:
    ManagerPool(QnMediaServerModule* serverModule);
    ~ManagerPool();
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

    using PluginList = QList<nx::sdk::metadata::Plugin*>;

    /**
     * @return List of available plugins; for each item, queryInterface() is called which increases
     * the reference counter, thus, it is usually needed to release each plugin in the caller.
     */
    PluginList availablePlugins() const;

    /**
     * @return Deserialized plugin manifest, or none on error.
     */
    boost::optional<nx::vms::api::analytics::PluginManifest> loadPluginManifest(
        nx::sdk::metadata::Plugin* plugin);

public slots:
    void initExistingResources();

private:
    using PixelFormat = nx::sdk::metadata::UncompressedVideoFrame::PixelFormat;

    std::unique_ptr<const nx::plugins::SettingsHolder> loadSettingsFromFile(
        const QString& fileDescription, const QString& filename);

    void saveManifestToFile(
        const char* manifest,
        const QString& fileDescription,
        const QString& pluginLibName,
        const QString& filenameExtraSuffix = "");

    void setCameraManagerDeclaredSettings(
        sdk::metadata::CameraManager* manager,
        const QnSecurityCamResourcePtr& camera,
        const QString& pluginLibName);

    void setPluginDeclaredSettings(
        sdk::metadata::Plugin* plugin,
        const QString& pluginLibName);

    void createCameraManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::CameraManager* createCameraManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::Plugin* plugin) const;

    void releaseResourceCameraManagersUnsafe(const QnSecurityCamResourcePtr& camera);

    void releaseResourceCameraManagersUnsafe(const QnUuid& resourceId);

    MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const nx::vms::api::analytics::PluginManifest& manifest);

    void handleResourceChanges(const QnResourcePtr& resource);

    bool isCameraAlive(const QnSecurityCamResourcePtr& camera) const;

    void fetchMetadataForResourceUnsafe(
        const QnUuid& resourceId,
        ResourceMetadataContext& context,
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
            NX_ERROR(this) << "Unable to deserialize plugin manifest:" << manifestString;
            return boost::none;
        }

        return deserialized;
    }

    void assignPluginManifestToServer(
        const nx::vms::api::analytics::PluginManifest& manifest,
        const QnMediaServerResourcePtr& server);

    nx::vms::api::analytics::PluginManifest mergePluginManifestToServer(
        const nx::vms::api::analytics::PluginManifest& manifest,
        const QnMediaServerResourcePtr& server);

    std::pair<
        boost::optional<nx::vms::api::analytics::CameraManagerManifest>,
        boost::optional<nx::vms::api::analytics::PluginManifest>
    >
    loadManagerManifest(
        const nx::sdk::metadata::Plugin* plugin,
        nx::sdk::metadata::CameraManager* manager,
        const QnSecurityCamResourcePtr& camera);

    void addManifestToCamera(
        const nx::vms::api::analytics::CameraManagerManifest& manifest,
        const QnSecurityCamResourcePtr& camera);

    bool cameraInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::CameraInfo* outCameraInfo) const;

    void putVideoFrame(
        const QnUuid& id,
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame);

    boost::optional<PixelFormat> pixelFormatFromManifest(
        const nx::vms::api::analytics::PluginManifest& manifest);

    void issueMissingUncompressedFrameWarningOnce();

private:
    ResourceMetadataContextMap m_contexts;
    QnMutex m_contextMutex;
    bool m_uncompressedFrameWarningIssued = false;
    nx::debugging::VisualMetadataDebuggerPtr m_visualMetadataDebugger;
    QThread* m_thread;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx

namespace nx {
namespace sdk {

QString toString(const nx::sdk::CameraInfo& cameraInfo);

} // namespace sdk
} // namespace nx
