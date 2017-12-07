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
#include <nx/sdk/metadata/abstract_metadata_plugin.h>
#include <nx/sdk/metadata/abstract_metadata_manager.h>
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
    PluginList availablePlugins() const;

    void createMetadataManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::AbstractMetadataManager* createMetadataManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::AbstractMetadataPlugin* plugin) const;

    void releaseResourceMetadataManagersUnsafe(const QnSecurityCamResourcePtr& camera);

    MetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const QnUuid& pluginId);

    void handleResourceChanges(const QnResourcePtr& resource);

    bool isCameraAlive(const QnSecurityCamResourcePtr& camera) const;

    void fetchMetadataForResourceUnsafe(ResourceMetadataContext& context, QSet<QnUuid>& eventTypeIds);

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

    boost::optional<nx::api::AnalyticsDriverManifest> addManifestToServer(
        nx::sdk::metadata::AbstractMetadataPlugin* plugin);

    boost::optional<nx::api::AnalyticsDeviceManifest> addManifestToCamera(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::AbstractMetadataManager* manager);

    bool resourceInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::ResourceInfo* outResourceInfo) const;

    void putVideoData(const QnUuid& id, const QnCompressedVideoData* data);
private:
    ResourceMetadataContextMap m_contexts;
    QnMediaServerModule* m_serverModule;
    QnMutex m_contextMutex;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
