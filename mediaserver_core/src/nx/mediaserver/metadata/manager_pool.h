#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/plugin.h>
#include <nx/sdk/metadata/camera_manager.h>
#include <nx/api/analytics/driver_manifest.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>
#include <nx/fusion/serialization/json.h>

class QnMediaServerModule;

namespace nx {
namespace mediaserver {
namespace metadata {

struct ResourceMetadataContext
{
public:
    using ManagerDeleter = std::function<void(nx::sdk::metadata::CameraManager*)>;

    ResourceMetadataContext(
        nx::sdk::metadata::CameraManager*,
        nx::sdk::metadata::MetadataHandler*);

    std::unique_ptr<nx::sdk::metadata::MetadataHandler> handler;
    std::unique_ptr<nx::sdk::metadata::CameraManager, ManagerDeleter> manager;
};

class ManagerPool final:
    public Connective<QObject>
{
    using ResourceMetadataContextMap = std::multimap<QnUuid, ResourceMetadataContext>;
    using PluginList = QList<nx::sdk::metadata::Plugin*>;

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

public slots:
    void initExistingResources();

private:
    PluginList availablePlugins() const;

    void createMetadataManagersForResource(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::CameraManager* createMetadataManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::Plugin* plugin) const;

    void releaseResourceMetadataManagers(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const nx::api::AnalyticsDriverManifest& manifest);

    void handleResourceChanges(const QnResourcePtr& resource);

    bool canFetchMetadataFromResource(const QnSecurityCamResourcePtr& camera) const;

    bool fetchMetadataForResource(const QnUuid& resourceId, QSet<QnUuid>& eventTypeIds);

    template<typename T>
    boost::optional<T> deserializeManifest(const char* manifestString) const
    {
        bool success = false;
        auto deserialized = QJson::deserialized<T>(
            QString(manifestString).toUtf8(),
            T(),
            &success);

        if (!success)
            return boost::none;

        return deserialized;
    }

    boost::optional<nx::api::AnalyticsDriverManifest> loadPluginManifest(
        nx::sdk::metadata::Plugin* plugin);

    void assignPluginManifestToServer(
        const nx::api::AnalyticsDriverManifest& manifest,
        const QnMediaServerResourcePtr& server);

    nx::api::AnalyticsDriverManifest mergePluginManifestToServer(
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

private:
    ResourceMetadataContextMap m_contexts;
    QnMediaServerModule* m_serverModule;
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
