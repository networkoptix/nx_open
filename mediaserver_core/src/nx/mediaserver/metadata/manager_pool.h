#pragma once

#include <QtCore/QObject>
#include <QtCore/QSet>

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <common/common_module_aware.h>
#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>
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
    using ManagerDeleter = std::function<void(nx::sdk::metadata::AbstractMetadataManager*)>;

    ResourceMetadataContext(
        nx::sdk::metadata::AbstractMetadataManager*,
        nx::sdk::metadata::AbstractMetadataHandler*);

    std::unique_ptr<nx::sdk::metadata::AbstractMetadataHandler> handler;

    std::unique_ptr<
        nx::sdk::metadata::AbstractMetadataManager,
        ManagerDeleter> manager;
};

class ManagerPool final:
    public Connective<QObject>
{
    using ResourceMetadataContextMap = std::multimap<QnUuid, ResourceMetadataContext>;
    using PluginList = QList<nx::sdk::metadata::AbstractMetadataPlugin*>;

    Q_OBJECT
public:
    ManagerPool(QnMediaServerModule* commonModule);
    ~ManagerPool();
    void init();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_propertyChanged(const QnResourcePtr& resource, const QString& name);
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

public slots:
    void initExistingResources();

private:
    PluginList availablePlugins() const;

    void createMetadataManagersForResource(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::AbstractMetadataManager* createMetadataManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::AbstractMetadataPlugin* plugin) const;

    void releaseResourceMetadataManagers(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::AbstractMetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const QnUuid& pluginId);

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
        nx::sdk::metadata::AbstractMetadataPlugin* plugin);

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
        nx::sdk::metadata::AbstractMetadataManager* manager,
        const QnSecurityCamResourcePtr& camera);

    void addManifestToCamera(
        const nx::api::AnalyticsDeviceManifest& manifest,
        const QnSecurityCamResourcePtr& camera);

    bool resourceInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::ResourceInfo* outResourceInfo) const;

private:
    ResourceMetadataContextMap m_contexts;
    QnMediaServerModule* m_serverModule;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
