#pragma once

#include <map>
#include <vector>

#include <boost/optional/optional.hpp>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_aware.h>

#include <plugins/metadata/abstract_metadata_manager.h>

#include <nx/api/analytics/driver_manifest.h>
#include <nx/mediaserver/metadata/rule_holder.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class MetadataFilter
{
};

struct ResourceMetadataContext
{
public:
    using ManagerDeleter = std::function<void(nx::sdk::metadata::AbstractMetadataManager*)>;

    ResourceMetadataContext(
        nx::sdk::metadata::AbstractMetadataManager*,
        nx::sdk::metadata::AbstractMetadataHandler*);
    
    std::unique_ptr<
        nx::sdk::metadata::AbstractMetadataManager,
        ManagerDeleter> manager;

    std::unique_ptr<nx::sdk::metadata::AbstractMetadataHandler> handler;
};

class ManagerPool: public nx::core::resource::ResourcePoolAware
{
    using ResourceMetadataContextMap = std::multimap<QnUuid, ResourceMetadataContext>;

public:
    ManagerPool(QnCommonModule* commonModule);
    virtual void resourceAdded(const QnResourcePtr& resource) override;
    virtual void resourceRemoved(const QnResourcePtr& resource) override;
    void rulesUpdated(const std::set<QnUuid>& affectedResources);

private:
    void createMetadataManagersForResource(const QnResourcePtr& resource);
    void releaseResourceMetadataManagers(const QnResourcePtr& resource);

    nx::sdk::metadata::AbstractMetadataHandler* createMetadataHandler(
        const QnResourcePtr& resource,
        const nx::api::AnalyticsDriverManifest& manager);

    void handleResourceChanges(const QnResourcePtr& resource);
    void handleResourceChangesUnsafe(const QnResourcePtr& resource);

    bool canFetchMetadataFromResource(const QnUuid& resourceId) const;

    bool fetchMetadataForResource(const QnUuid& resourceId, std::set<QnUuid>& eventTypeIds);

    boost::optional<nx::api::AnalyticsDriverManifest> deserializeManifest(
        const char* manifestString) const;

    bool resourceInfoFromResource(
        const QnResourcePtr& resource,
        nx::sdk::metadata::ResourceInfo* outResourceInfo) const;

private:
    mutable QnMutex m_mutex;
    ResourceMetadataContextMap m_contexts;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx