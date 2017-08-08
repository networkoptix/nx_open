#include "metadata_manager_pool.h"

#include <common/common_module.h>
#include <plugins/plugin_manager.h>

namespace nx {
namespace analytics {

using namespace nx::sdk;
using namespace nx::core;

MetadataManagerPool::MetadataManagerPool(QnCommonModule* commonModule):
    resource::ResourcePoolAware(commonModule)
{

}

void MetadataManagerPool::resourceAdded(const QnResourcePtr& resource)
{
    createMetadataManagersForResource(resource);
}

void MetadataManagerPool::resourceRemoved(const QnResourcePtr& resource)
{
    releaseResourceMetadataManagers(resource);
}

void MetadataManagerPool::fetchMetadataWithFilter(
    const QnResourcePtr& resource,
    const MetadataFilter& metadataTypes)
{
}

metadata::ResourceInfo MetadataManagerPool::resourceInfoFromResource(const QnResourcePtr& resource) const
{
    return metadata::ResourceInfo();
}

void MetadataManagerPool::createMetadataManagersForResource(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    auto resourceInfo = resourceInfoFromResource(resource);

    auto pluginManager = commonModule()->pluginManager();
    NX_ASSERT(pluginManager, lit("Can not access PluginManager instance"));
    if (!pluginManager)
        return;

    auto plugins = pluginManager->findNxPlugins<metadata::AbstractMetadataPlugin>(
        metadata::IID_MetadataManager);

    for (auto& plugin: plugins)
    {
        metadata::Error error = metadata::Error::noError;
        auto manager = plugin->managerForResource(resourceInfo, &error);

        if (manager)
            m_metadataManagersByResource.emplace(resource->getId(), manager);
    }
}

void MetadataManagerPool::releaseResourceMetadataManagers(const QnResourcePtr& resource)
{
    QnMutexLocker lock(&m_mutex);
    auto id = resource->getId();
    auto range  = m_metadataManagersByResource.equal_range(id);
    auto itr = range.first;

    while (itr != range.second)
    {
        itr->second->releaseRef();
        ++itr;
    }

    m_metadataManagersByResource.erase(id);
}

} // namespace analytics
} // namespace nx