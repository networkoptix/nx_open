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
#include <core/dataconsumer/abstract_data_receptor.h>

class QnMediaServerModule;
class QnAbstractMediaStreamDataProvider;

namespace nx {
namespace mediaserver {
namespace metadata {

class EventHandler;
class VideoDataReceptor;

struct ResourceMetadataContext: public QnAbstractDataReceptor
{
public:
    ResourceMetadataContext();
    ~ResourceMetadataContext();

    void setManager(nx::sdk::metadata::AbstractMetadataManager* manager);
    void setHandler(nx::sdk::metadata::AbstractMetadataHandler* handler);
    void setDataProvider(const QnAbstractMediaStreamDataProvider* provider);
    void setVideoFrameDataReceptor(const QSharedPointer<VideoDataReceptor>& receptor);
    void setMetadataDataReceptor(QnAbstractDataReceptor* receptor);

    nx::sdk::metadata::AbstractMetadataManager* manager() const;
    nx::sdk::metadata::AbstractMetadataHandler* handler() const;
    const QnAbstractMediaStreamDataProvider* dataProvider() const;
    QSharedPointer<VideoDataReceptor> videoFrameDataReceptor() const;
    QnAbstractDataReceptor* metadataDataReceptor() const;
protected:
    virtual bool canAcceptData() const override;
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

private:
    mutable QnMutex m_mutex;
    std::unique_ptr<nx::sdk::metadata::AbstractMetadataManager> m_manager;
    std::unique_ptr<nx::sdk::metadata::AbstractMetadataHandler> m_handler;

    const QnAbstractMediaStreamDataProvider* m_dataProvider;
    QSharedPointer<VideoDataReceptor> m_videoFrameDataReceptor;
    QnAbstractDataReceptor* m_metadataReceptor;
};

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
    void at_resourceRemoved(const QnResourcePtr& resource);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

    QWeakPointer<QnAbstractDataReceptor> registerDataProvider(QnAbstractMediaStreamDataProvider* dataProvider);
    void removeDataProvider(QnAbstractMediaStreamDataProvider* dataProvider);

    void registerDataReceptor(const QnResourcePtr& resource, QnAbstractDataReceptor* dataReceptor);
    void removeDataReceptor(const QnResourcePtr& resource, QnAbstractDataReceptor* dataReceptor);

public slots:
    void initExistingResources();

private:
    PluginList availablePlugins() const;

    void createMetadataManagersForResourceUnsafe(const QnSecurityCamResourcePtr& camera);

    nx::sdk::metadata::AbstractMetadataManager* createMetadataManager(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::AbstractMetadataPlugin* plugin) const;

    void releaseResourceMetadataManagersUnsafe(const QnSecurityCamResourcePtr& camera);

    EventHandler* createMetadataHandler(
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

    boost::optional<nx::api::AnalyticsDriverManifest> addManifestToServer(
        nx::sdk::metadata::AbstractMetadataPlugin* plugin);

    boost::optional<nx::api::AnalyticsDeviceManifest> addManifestToCamera(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::metadata::AbstractMetadataManager* manager);

    bool resourceInfoFromResource(
        const QnSecurityCamResourcePtr& camera,
        nx::sdk::ResourceInfo* outResourceInfo) const;

private:
    ResourceMetadataContextMap m_contexts;
    QnMediaServerModule* m_serverModule;
    QnMutex m_contextMutex;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
