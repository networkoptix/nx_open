#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <nx/utils/log/log.h>
#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/fusion/serialization/json.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include "resource_metadata_context.h"

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx {
namespace mediaserver {
namespace metadata {

class MetadataHandler;

class ManagerPool final:
    public Connective<QObject>
{
    using ResourceMetadataContextMap = std::map<QnUuid, ResourceMetadataContext>;

    Q_OBJECT
public:
    ManagerPool(QnMediaServerModule* commonModule);
    ~ManagerPool();
    void init();
    void stop();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_propertyChanged(const QnResourcePtr& resource, const QString& name);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

    /**
     * Return QnAbstractDataReceptor that will receive data from audio/video data provider.
     */
    QWeakPointer<QnAbstractDataReceptor> mediaDataReceptor(const QnUuid& id);

    /**
     * Register data receptor that will receive metadata packets.
     */
    void registerDataReceptor(const QnResourcePtr& resource, QWeakPointer<QnAbstractDataReceptor> metadaReceptor);

public slots:
    void initExistingResources();

private:
    using PluginList = QList<nx::sdk::metadata::Plugin*>;

    PluginList availablePlugins() const;

    void loadSettingsFromFile(std::vector<nxpl::Setting>* settings, const char* filename);

    void setManagerDeclaredSettings(
        sdk::metadata::CameraManager* manager, const QnSecurityCamResourcePtr& camera);

    void setPluginDeclaredSettings(sdk::metadata::Plugin* plugin);

    void createCameraManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::CameraManager* createCameraManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::Plugin* plugin) const;

    void releaseResourceCameraManagersUnsafe(const QnSecurityCamResourcePtr& camera);

    MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const QnUuid& pluginId);

    void handleResourceChanges(const QnResourcePtr& resource);

    bool isCameraAlive(const QnSecurityCamResourcePtr& camera) const;

    void fetchMetadataForResourceUnsafe(
        const QnUuid& resourceId,
        ResourceMetadataContext& context,
        QSet<QnUuid>& eventTypeIds);

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

    boost::optional<nx::api::AnalyticsDriverManifest> loadPluginManifest(
        nx::sdk::metadata::Plugin* plugin);

    void assignPluginManifestToServer(
        const nx::api::AnalyticsDriverManifest& manifest,
        const QnMediaServerResourcePtr& server);

    void mergePluginManifestToServer(
        const nx::api::AnalyticsDriverManifest& manifest,
        const QnMediaServerResourcePtr& server);

    std::pair<
        boost::optional<nx::api::AnalyticsDeviceManifest>,
        boost::optional<nx::api::AnalyticsDriverManifest>
    >
    loadManagerManifest(
        nx::sdk::metadata::CameraManager* manager,
        const QnSecurityCamResourcePtr& camera);

    void addManifestToCamera(
        const nx::api::AnalyticsDeviceManifest& manifest,
        const QnSecurityCamResourcePtr& camera);

    bool cameraInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::CameraInfo* outCameraInfo) const;

    void putVideoData(const QnUuid& id, const QnCompressedVideoData* data);
private:
    ResourceMetadataContextMap m_contexts;
    QnMediaServerModule* m_serverModule;
    QnMutex m_contextMutex;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
