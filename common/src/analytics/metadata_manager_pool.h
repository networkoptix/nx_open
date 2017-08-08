#pragma once

#include <map>
#include <vector>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool_aware.h>

#include <plugins/metadata/abstract_metadata_manager.h>

namespace nx {
namespace analytics {

class MetadataFilter
{
};

class MetadataManagerPool: public nx::core::resource::ResourcePoolAware
{
    using MetadataManagerMap = std::multimap<QnUuid, nx::sdk::metadata::AbstractMetadataManager*>;

public:
    MetadataManagerPool(QnCommonModule* commonModule);
    virtual void resourceAdded(const QnResourcePtr& resource) override;
    virtual void resourceRemoved(const QnResourcePtr& resource) override;

    void fetchMetadataWithFilter(
        const QnResourcePtr& resource,
        const MetadataFilter& metadataTypes);

private:
    nx::sdk::metadata::ResourceInfo resourceInfoFromResource(const QnResourcePtr& resource) const;
    void createMetadataManagersForResource(const QnResourcePtr& resource);
    void releaseResourceMetadataManagers(const QnResourcePtr& resource);

private:
    mutable QnMutex m_mutex;
     MetadataManagerMap m_metadataManagersByResource;
};


} // namespace analytics
} // namespace nx